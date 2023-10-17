/**
 * @file   fluid1d.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.06
 *
 * @brief  Fluid solver for 1d fluids
 */

#include "fluid1d.h"
#include "utils/enumerate.h"
#include "utils/zip.h"

#include <glm/glm.hpp>
#include <glm/ext/scalar_common.hpp>
#include <numeric>
#include <execution>
#include <ranges>

namespace wavy
{
    namespace detail
    {
        template<typename T>
        concept Increasable = requires(T a, T b) {
            { a += b } -> std::convertible_to<T>;
        };

        template<Increasable T>
        struct iota_step
        {
            iota_step(const T& init, const T& inc)
                : val{init}
                , increase{ inc }
            {
            }

            operator T() const { return val; }
            iota_step& operator++()
            {
                val += increase;
                return *this;
            }

            T val;
            T increase;
        };
    }

    FluidSolver1D::FluidSolver1D(std::size_t grid_size, float delta_x, float g, float density)
        : FluidSolverBase{grid_size,
                          [](const std::span<Label>&, [[maybe_unused]] std::size_t idx) {
                              return Label::SOLID;
                          }}
        , m_delta_x{delta_x}
        , m_g{g}
        , m_density{density}
        , m_position(grid_size, 0.0f)
        , m_p(grid_size, 0.0f)
        , m_u_n0(grid_size + 1, 0.0f)
        , m_u_A(grid_size + 1, 0.0f)
        , m_u_B(grid_size + 1, 0.0f)
        , m_u_n1(grid_size + 1, 0.0f)
        , m_rhs(grid_size, 0.0f)
        , m_A_diag(grid_size, 0.0f)
        , m_A_x(grid_size, 0.0f)
    {
        std::iota(std::begin(m_position), std::end(m_position), detail::iota_step<float>(0.0f, m_delta_x));
    }

    void FluidSolver1D::solveNextStep(float delta_t_frame)
    {
        bool continue_simulation = true;
        while (continue_simulation) {
            // determine deltaT
            auto delta_t = glm::min(estimateAdvectionDeltaT(), estimateBodyForcesDeltaT(), estimateProjectDeltaT());
            delta_t = std::max(delta_t, delta_t_frame / 3.0f);
            if (delta_t > delta_t_frame) {
                delta_t = delta_t_frame;
                continue_simulation = false;
            }

            advect(delta_t, m_u_n0, m_u_A);
            bodyForces(delta_t, m_u_A, m_u_B);
            // project(deltaT, m_u_B, m_un1);

            // auto enumerator = utils::enumerate(m_indices_data);
            // std::for_each(std::execution::par, std::begin(enumerator), std::end(enumerator), []([[maybe_unused]] const auto& v) {});
            // auto zipper = utils::zip(m_indices_data, m_p, m_u_A);
            // auto zipper2 = zipper;
            // std::for_each(std::execution::par, std::begin(zipper), std::end(zipper),
            //               []([[maybe_unused]] const auto& v) {});
        }
    }

    void FluidSolver1D::advect(float delta_t, const std::vector<float>& qn0, std::vector<float>& qn1) const
    {
        auto zipped_data = utils::zip(m_position, qn1);
        std::for_each(std::execution::par, std::begin(zipped_data), std::end(zipped_data),
                      [this, &qn0, delta_t](auto zipped_element) {
                          auto xG = std::get<0>(zipped_element);
                          auto xP = integrate(
                              utils::boundary_span<const float>{m_u_n0,
                                                                [](const std::span<const float>& u, std::size_t idx) {
                                                                    return u[glm::clamp(idx, 0ULL, u.size())];
                                                                }},
                              xG, delta_t);
                          std::get<1>(zipped_element) = interpolate(
                              utils::boundary_span<const float>{qn0,
                                                                [](std::span<const float> q, std::size_t idx) {
                                                                    return q[glm::clamp(idx, 0ULL, q.size())];
                                                                }},
                              xP);
                      });
    }

    void FluidSolver1D::bodyForces(float delta_t, const std::vector<float>& qn0, std::vector<float>& qn1) const
    {
        auto zipped_data = utils::zip(qn0, qn1);
        std::for_each(std::execution::par, std::begin(zipped_data), std::end(zipped_data),
                      [delta_t](auto zipped_element) {
                          std::get<1>(zipped_element) = std::get<0>(zipped_element) + delta_t * 9.81f;
                      });
    }

    void FluidSolver1D::project([[maybe_unused]] float delta_t, const std::vector<float>& qn0,
                                [[maybe_unused]] std::vector<float>& qn1,
                                mysh::core::function_view<float(std::size_t idx)> u_solid)
    {
        presure_gradient_rhs(qn0, m_rhs, u_solid);
    }

    float FluidSolver1D::estimateAdvectionDeltaT() const
    {
        auto maxu = std::reduce(std::execution::par, std::begin(m_u_n0), std::end(m_u_n0), 0.0f,
                                [](float v0, float v1) { return std::max(v0, v1); });
        auto umax = maxu + glm::sqrt(5.0f * m_delta_x * m_g);
        return (5.0f * m_delta_x) / umax;
    }

    float FluidSolver1D::estimateBodyForcesDeltaT() const
    {
        return 1.0f;
    }

    float FluidSolver1D::estimateProjectDeltaT() const
    {
        return 1.0f;
    }

    void FluidSolver1D::presure_gradient_rhs(const std::vector<float>& u, std::vector<float>& rhs,
                                             mysh::core::function_view<float(std::size_t idx)> u_solid) const
    {
        auto enumerated_data = utils::enumerate(rhs);
        auto scale = 1.0f / m_delta_x;
        std::for_each(std::execution::par, std::begin(enumerated_data), std::end(enumerated_data),
                      [this, &u, &u_solid, scale](auto enum_element) {
                          auto index = std::get<0>(enum_element);
                          auto& result = std::get<1>(enum_element);
                          if (m_labels[index] != FluidSolverBase::Label::FLUID) return;
                          result = -scale * (u[index + 1] - u[index]);
                          if (m_labels[index - 1] == FluidSolverBase::Label::SOLID) {
                              result -= scale * (u[index] - u_solid(index));
                          }
                          if (m_labels[index + 1] == FluidSolverBase::Label::SOLID) {
                              result += scale * (u[index + 1] - u_solid(index + 1));
                          }
                      });
    }

    void FluidSolver1D::setup_A(float delta_t)
    {
        auto zipped_data = utils::zip(utils::enumerate(m_A_diag), m_A_x);

        auto scale = delta_t / (m_density * m_delta_x * m_delta_x);
        std::for_each(std::execution::par, std::begin(zipped_data), std::end(zipped_data),
                      [this, scale](auto zipped_element) {
                          auto index = std::get<0>(std::get<0>(zipped_element));
                          auto& A_diag = std::get<1>(std::get<0>(zipped_element));
                          auto& A_x = std::get<1>(zipped_element);
                          A_diag = 0.0f;
                          A_x = 0.0f;
                          if (m_labels[index] != FluidSolverBase::Label::FLUID) return;
                          if (m_labels[index - 1] == FluidSolverBase::Label::FLUID) { A_diag += scale; }
                          if (m_labels[index + 1] == FluidSolverBase::Label::FLUID) {
                              A_diag += scale;
                              A_x = -scale;
                          } else if (m_labels[index + 1] == FluidSolverBase::Label::EMPTY) {
                              A_diag += scale;
                          }
                      });
    }

    float FluidSolver1D::toPosition(std::size_t index) const
    {
        return m_delta_x * static_cast<float>(index);
    }

    std::size_t FluidSolver1D::toGrid(float position) const
    {
        return static_cast<std::size_t>(position / m_delta_x);
    }

    float FluidSolver1D::interpolate(const utils::boundary_span<const float>& q, float x_P) const
    {
        return FluidSolverBase::interpolate(q, x_P, m_delta_x);
    }

    float FluidSolver1D::integrate(const utils::boundary_span<const float>& f, float q, float delta_t) const
    {
        return FluidSolverBase::integrate(f, q, delta_t, m_delta_x);
    }
}

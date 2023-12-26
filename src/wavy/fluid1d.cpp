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

#include <eigen3/Eigen/Core>
#include <glm/glm.hpp>
#include <glm/ext/scalar_common.hpp>
#include <spdlog/spdlog.h>
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
            iota_step(const T& init, const T& inc) // NOLINT(bugprone-easily-swappable-parameters)
                : val{init}
                , increase{ inc }
            {
            }

            operator T() const { return val; } // NOLINT(hicpp-explicit-conversions)
            iota_step& operator++()
            {
                val += increase;
                return *this;
            }

            T val;
            T increase;
        };
    }

    FluidSolver1D::FluidSolver1D(std::size_t grid_size, float delta_x, float g, float density) // NOLINT(bugprone-easily-swappable-parameters)
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
        , m_precon(grid_size, 0.0f)
        , m_b(grid_size, 0.0f)
        , m_r(grid_size, 0.0f)
        , m_q(grid_size, 0.0f)
        , m_z(grid_size, 0.0f)
        , m_s(grid_size, 0.0f)
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

    void FluidSolver1D::bodyForces(float delta_t, const std::vector<float>& qn0, std::vector<float>& qn1) const // NOLINT(readability-convert-member-functions-to-static)
    {
        auto zipped_data = utils::zip(qn0, qn1);
        std::for_each(std::execution::par, std::begin(zipped_data), std::end(zipped_data),
                      [delta_t](auto zipped_element) {
                          constexpr float gravityOfEarth = 9.81f;
                          std::get<1>(zipped_element) = std::get<0>(zipped_element) + delta_t * gravityOfEarth;
                      });
    }

    void FluidSolver1D::project([[maybe_unused]] float delta_t, const std::vector<float>& qn0,
                                [[maybe_unused]] std::vector<float>& qn1,
                                mysh::core::function_view<float(std::size_t idx)> u_solid)
    {
        presure_gradient_rhs(qn0, m_rhs, u_solid);
        setup_A(delta_t);
        // TODO: construct preconditioner
        // TODO: solve Ap=b
        // TODO: compute new velocities u^n+1
    }

    float FluidSolver1D::estimateAdvectionDeltaT() const
    {
        constexpr float estimation_factor = 5.0f;
        auto maxu = std::reduce(std::execution::par, std::begin(m_u_n0), std::end(m_u_n0), 0.0f,
                                [](float v0, float v1) { return std::max(v0, v1); });
        auto umax = maxu + glm::sqrt(estimation_factor * m_delta_x * m_g);
        return (estimation_factor * m_delta_x) / umax;
    }

    float FluidSolver1D::estimateBodyForcesDeltaT() const // NOLINT(readability-convert-member-functions-to-static)
    {
        return 1.0f;
    }

    float FluidSolver1D::estimateProjectDeltaT() const // NOLINT(readability-convert-member-functions-to-static)
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
                          if (labels()[index] != Label::FLUID) { return; }
                          result = -scale * (u[index + 1] - u[index]);
                          if (labels()[index - 1] == Label::SOLID) {
                              result -= scale * (u[index] - u_solid(index));
                          }
                          if (labels()[index + 1] == Label::SOLID) {
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
                          if (labels()[index] != Label::FLUID) { return; }
                          if (labels()[index - 1] == Label::FLUID) { A_diag += scale; }
                          if (labels()[index + 1] == Label::FLUID) {
                              A_diag += scale;
                              A_x = -scale;
                          } else if (labels()[index + 1] == Label::EMPTY) {
                              A_diag += scale;
                          }
                      });
    }

    void FluidSolver1D::incomplete_cholesky(const std::vector<float>& A_diag, const std::vector<float>& A_x,
                                            std::vector<float>& precon) const
    {
        // constexpr float tau = 0.97f; unused for 1D case
        constexpr float sigma = 0.25f;
        for (std::size_t i = 0; i < A_diag.size(); ++i) {
            if (labels()[i] != Label::FLUID) continue;
            auto e_root = 0.0f;
            if (i > 0) e_root = (A_x[i - 1] * precon[i - 1]);
            auto e = A_diag[i] - e_root * e_root;
            if (e < sigma * A_diag[i]) e = A_diag[i];

            precon[i] = 1.0f / glm::sqrt(e);
        }
    }

    void FluidSolver1D::preconditioned_conjugate_gradient()
    {
        std::fill(std::begin(m_p), std::end(m_p), 0.0f);
        if (std::all_of(std::begin(m_b), std::end(m_b), [](auto v) { return v == 0.0f; })) { return; }
        m_r = m_b;
        apply_preconditioner(m_A_x, m_precon, m_r, m_z, m_q);

        Eigen::Map<Eigen::VectorXf> ez{m_z.data(), static_cast<Eigen::Index>(m_z.size())};
        Eigen::Map<Eigen::VectorXf> er{m_r.data(), static_cast<Eigen::Index>(m_r.size())};
        Eigen::Map<Eigen::VectorXf> es{m_s.data(), static_cast<Eigen::Index>(m_s.size())};
        Eigen::Map<Eigen::VectorXf> ep{m_p.data(), static_cast<Eigen::Index>(m_p.size())};

        float sigma = ez.dot(er);

        constexpr int max_iterations = 10;
        constexpr float tol = 1.e-6f;
        for (int i = 0; i < max_iterations; ++i) {
            apply_A(m_A_diag, m_A_x, m_s, m_z);
            auto alpha = sigma / ez.dot(es);
            ep += alpha * es;
            er -= alpha * ez;
            if (er.maxCoeff() <= tol) { return; }
            apply_preconditioner(m_A_x, m_precon, m_r, m_z, m_q);
            auto sigma_new = ez.dot(er);
            auto beta = sigma_new / sigma;
            es = ez + beta * es;
            sigma = sigma_new;
        }

        spdlog::debug("PCG stopped after {} iterations.", max_iterations);
    }

    void FluidSolver1D::apply_preconditioner(const std::vector<float>& A_x, const std::vector<float>& precon,
                                             const std::vector<float>& r, std::vector<float>& z,
                                             std::vector<float>& q) const
    {
        for (std::size_t i = 0; i < A_x.size(); ++i) {
            if (labels()[i] != Label::FLUID) continue;
            if (i == 0) {
                q[i] = 0.0f;
                continue;
            }

            auto t = r[i] - A_x[i - 1] * precon[i - 1] * q[i - 1];
            q[i] = t * precon[i];
        }

        for (std::size_t i = 0; i < A_x.size(); ++i) {
            if (labels()[i] != Label::FLUID) continue;
            auto t = q[i] - A_x[i] * precon[i] * z[i + 1];
            z[i] = t * precon[i];
        }
    }

    void FluidSolver1D::apply_A(const std::vector<float>& A_diag, const std::vector<float>& A_x, const std::vector<float>& s,
                                std::vector<float>& z) const
    {

        auto enumerated_data = utils::enumerate(utils::zip(A_diag, z));

        std::for_each(std::execution::par, std::begin(enumerated_data), std::end(enumerated_data),
                      [this, &A_x, &s](auto enum_element) {
                          auto i = std::get<0>(enum_element);
                          auto& A_ii = std::get<0>(std::get<1>(enum_element));
                          auto& z_i = std::get<1>(std::get<1>(enum_element));

                          z_i = A_ii * s[i];
                          if (i > 0) { z_i += A_x[i - 1] * s[i - 1]; }
                          if (i < s.size() - 1) { z_i += A_x[i + 1] * s[i + 1]; }
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
        return FluidSolverBase::Interpolate(q, x_P, m_delta_x);
    }

    float FluidSolver1D::integrate(const utils::boundary_span<const float>& f, float q, float delta_t) const
    {
        return FluidSolverBase::Integrate(f, q, delta_t, m_delta_x);
    }
}

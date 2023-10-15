/**
 * @file   fluid_base.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.07
 *
 * @brief  Fluid solver base class.
 */

#include "fluid_base.h"
#include "app_constants.h"

#include <glm/glm.hpp>
#include <numeric>

namespace fluid {
    FluidSolverBase::FluidSolverBase(
        std::size_t grid_size,
        const std::function<Label(const std::span<Label>&, std::size_t)>& labels_handler)
        : m_labels_data(grid_size, Label::FLUID)
        , m_labels{m_labels_data, labels_handler}
    {
    }

    float FluidSolverBase::interpolateLinear(const utils::boundary_span<const float>& q, float s, std::size_t xi) const
    {
        return glm::mix(q[xi], q[xi + 1], s);
    }

    float FluidSolverBase::interpolateCubic(const utils::boundary_span<const float>& q, float s, std::size_t xi) const
    {
        auto s2 = s * s;
        auto s3 = s2 * s;
        auto w_1 = (-1.0f / 3.0f) * s + 0.5f * s2 - (1.0f / 6.0f) * s3;
        auto w0 = 1.0f - s2 + 0.5f * (s3 - s);
        auto w1 = s + 0.5f * (s2 - s3);
        auto w2 = (1.0f / 6.0f * (s3 - s));
        return w_1 * q[xi - 1] + w0 * q[xi] + w1 * q[xi + 1] + w2 * q[xi + 2];
    }

    float FluidSolverBase::interpolate(const utils::boundary_span<const float>& q, float x_P, float delta_x) const
    {
        auto x = x_P / delta_x;
        auto xi_f = glm::floor(x_P / delta_x);
        auto xi = static_cast<std::size_t>(xi_f);
        auto alpha = xi_f - x;
        if constexpr (interpolation_method == InterpolationMethod::Linear) return interpolateLinear(q, alpha, xi);
        if constexpr (interpolation_method == InterpolationMethod::Cubic) return interpolateCubic(q, alpha, xi);
    }

    float FluidSolverBase::integrateRG2(const utils::boundary_span<const float>& f, float q, float delta_t,
                                        float delta_x) const
    {
        auto qMid = q - 0.5f * delta_t * interpolate(f, q, delta_x);
        return q - delta_t * interpolate(f, qMid, delta_x);
    }

    float FluidSolverBase::integrateRG3(const utils::boundary_span<const float>& f, float q, float delta_t,
                                        float delta_x) const
    {
        auto k1 = interpolate(f, q, delta_x);
        auto k2 = interpolate(f, q - 0.5f * delta_t * k1, delta_x);
        auto k3 = interpolate(f, q - 0.75f * delta_t * k2, delta_x);
        return q - (delta_t / 9.0f) * (2.0f * k1 + 3.0f * k2 + 4.0f * k3);
    }

    float FluidSolverBase::integrateRG4(const utils::boundary_span<const float>& f, float q, float delta_t,
                                        float delta_x) const
    {
        auto k1 = interpolate(f, q, delta_x);
        auto k2 = interpolate(f, q - 0.5f * delta_t * k1, delta_x);
        auto k3 = interpolate(f, q - 0.5f * delta_t * k2, delta_x);
        auto k4 = interpolate(f, q - delta_t * k3, delta_x);
        return q - (delta_t / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
    }

    float FluidSolverBase::integrate(const utils::boundary_span<const float>& f, float q, float delta_t,
                                     float delta_x) const
    {
        using enum fluid::IntegrationMethod;
        if constexpr (integration_method == RK2) return integrateRG2(f, q, delta_t, delta_x);
        if constexpr (integration_method == RK3) return integrateRG3(f, q, delta_t, delta_x);
        if constexpr (integration_method == RK4) return integrateRG4(f, q, delta_t, delta_x);
    }
}

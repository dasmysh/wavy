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

namespace wavy
{
    namespace detail
    {
        // some constants used for Runge Kutta integration.
        constexpr float one_sixth = 1.0f / 6.0f;
        constexpr float one_nineth = 1.0f / 9.0f;
        constexpr float three_fourth = 0.75f;
    }

    FluidSolverBase::FluidSolverBase(
        std::size_t grid_size,
        const std::function<Label(const std::span<Label>&, std::size_t)>& labels_handler)
        : m_labels_data(grid_size, Label::FLUID)
        , m_labels{m_labels_data, labels_handler}
    {
    }

    float FluidSolverBase::InterpolateLinear(const utils::boundary_span<const float>& q, float s,
                                             std::size_t xi) // NOLINT(bugprone-easily-swappable-parameters)
    {
        return glm::mix(q[xi], q[xi + 1], s);
    }

    float FluidSolverBase::InterpolateCubic(const utils::boundary_span<const float>& q, float s, std::size_t xi) // NOLINT(bugprone-easily-swappable-parameters)
    {
        auto s2 = s * s;
        auto s3 = s2 * s;
        auto w_1 = (-1.0f / 3.0f) * s + 0.5f * s2 - (detail::one_sixth)*s3;
        auto w0 = 1.0f - s2 + 0.5f * (s3 - s);
        auto w1 = s + 0.5f * (s2 - s3);
        auto w2 = (detail::one_sixth * (s3 - s));
        return w_1 * q[xi - 1] + w0 * q[xi] + w1 * q[xi + 1] + w2 * q[xi + 2];
    }

    float FluidSolverBase::Interpolate(const utils::boundary_span<const float>& q, float x_P, float delta_x)
    {
        auto x = x_P / delta_x;
        auto xi_f = glm::floor(x_P / delta_x);
        auto xi = static_cast<std::size_t>(xi_f);
        auto alpha = xi_f - x;
        if constexpr (interpolation_method == InterpolationMethod::Linear) { return InterpolateLinear(q, alpha, xi); }
        if constexpr (interpolation_method == InterpolationMethod::Cubic) { return InterpolateCubic(q, alpha, xi); }
    }

    float FluidSolverBase::IntegrateRG2(const utils::boundary_span<const float>& f, float q, float delta_t,
                                        float delta_x)
    {
        auto qMid = q - 0.5f * delta_t * Interpolate(f, q, delta_x);
        return q - delta_t * Interpolate(f, qMid, delta_x);
    }

    float FluidSolverBase::IntegrateRG3(const utils::boundary_span<const float>& f, float q, float delta_t,
                                        float delta_x)
    {
        auto k1 = Interpolate(f, q, delta_x);
        auto k2 = Interpolate(f, q - 0.5f * delta_t * k1, delta_x);
        auto k3 = Interpolate(f, q - detail::three_fourth * delta_t * k2, delta_x);
        return q - (delta_t * detail::one_nineth) * (2.0f * k1 + 3.0f * k2 + 4.0f * k3);
    }

    float FluidSolverBase::IntegrateRG4(const utils::boundary_span<const float>& f, float q, float delta_t,
                                        float delta_x)
    {
        auto k1 = Interpolate(f, q, delta_x);
        auto k2 = Interpolate(f, q - 0.5f * delta_t * k1, delta_x);
        auto k3 = Interpolate(f, q - 0.5f * delta_t * k2, delta_x);
        auto k4 = Interpolate(f, q - delta_t * k3, delta_x);
        return q - (delta_t * detail::one_sixth) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
    }

    float FluidSolverBase::Integrate(const utils::boundary_span<const float>& f, float q, float delta_t,
                                     float delta_x)
    {
        using enum IntegrationMethod;
        if constexpr (integration_method == RK2) { return IntegrateRG2(f, q, delta_t, delta_x); }
        if constexpr (integration_method == RK3) { return IntegrateRG3(f, q, delta_t, delta_x); }
        if constexpr (integration_method == RK4) { return IntegrateRG4(f, q, delta_t, delta_x); }
    }
}

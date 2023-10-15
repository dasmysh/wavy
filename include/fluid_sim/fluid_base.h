/**
 * @file   fluid_base.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.06
 *
 * @brief  Fluid solver base class.
 */

#pragma once

#include "utils/boundary_span.h"

namespace fluid
{
    class FluidSolverBase
    {
    public:

    protected:
        enum class Label : std::uint8_t
        {
            FLUID,
            SOLID,
            EMPTY
        };

        FluidSolverBase(
            std::size_t grid_size,
            const std::function<Label(const std::span<Label>&, std::size_t)>& labels_handler);

        float interpolateLinear(const utils::boundary_span<const float>& q, float alpha, std::size_t xi) const;
        float interpolateCubic(const utils::boundary_span<const float>& q, float alpha, std::size_t xi) const;
        float interpolate(const utils::boundary_span<const float>& q, float x_P, float delta_x) const;

        float integrateRG2(const utils::boundary_span<const float>& f, float q, float delta_t, float delta_x) const;
        float integrateRG3(const utils::boundary_span<const float>& f, float q, float delta_t, float delta_x) const;
        float integrateRG4(const utils::boundary_span<const float>& f, float q, float delta_t, float delta_x) const;
        float integrate(const utils::boundary_span<const float>& f, float q, float delta_t, float delta_x) const;

        std::vector<Label> m_labels_data;
        utils::boundary_span<Label> m_labels;

    private:

    };
}

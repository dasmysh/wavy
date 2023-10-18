/**
 * @file   fluid_base.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.06
 *
 * @brief  Fluid solver base class.
 */

#pragma once

#include "utils/boundary_span.h"

namespace wavy
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

        struct LabeledCellIndex
        {
            std::size_t index : 24;
            Label label;
            LabeledCellIndex& operator++()
            {
                index += 1;
                return *this;
            }
        };

        FluidSolverBase(
            std::size_t grid_size,
            const std::function<Label(const std::span<Label>&, std::size_t)>& labels_handler);

        [[nodiscard]] static float InterpolateLinear(const utils::boundary_span<const float>& q, float s, std::size_t xi);
        [[nodiscard]] static float InterpolateCubic(const utils::boundary_span<const float>& q, float s, std::size_t xi);
        [[nodiscard]] static float Interpolate(const utils::boundary_span<const float>& q, float x_P, float delta_x);

        [[nodiscard]] static float IntegrateRG2(const utils::boundary_span<const float>& f, float q, float delta_t, float delta_x);
        [[nodiscard]] static float IntegrateRG3(const utils::boundary_span<const float>& f, float q, float delta_t, float delta_x);
        [[nodiscard]] static float IntegrateRG4(const utils::boundary_span<const float>& f, float q, float delta_t, float delta_x);
        [[nodiscard]] static float Integrate(const utils::boundary_span<const float>& f, float q, float delta_t, float delta_x);

        [[nodiscard]] const std::vector<Label>& labels_data() const { return m_labels_data; }
        [[nodiscard]] std::vector<Label>& labels_data() { return m_labels_data; }
        [[nodiscard]] const utils::boundary_span<Label>& labels() const { return m_labels; }
        [[nodiscard]] utils::boundary_span<Label>& labels() { return m_labels; }

    private:
        std::vector<Label> m_labels_data;
        utils::boundary_span<Label> m_labels;

    };
}

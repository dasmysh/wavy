/**
 * @file   fluid1d.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.06
 *
 * @brief  Fluid solver for 1d fluids
 */

#pragma once

#include "fluid_base.h"
#include "core/function_view.h"

#include <vector>

namespace wavy
{
    class FluidSolver1D : public FluidSolverBase
    {
    public:
        FluidSolver1D(std::size_t grid_size, float delta_x, float g, float density);

        void solveNextStep(float delta_t_frame);

    protected:
        void advect(float delta_t, const std::vector<float>& qn0, std::vector<float>& qn1) const;
        void bodyForces(float delta_t, const std::vector<float>& qn0, std::vector<float>& qn1) const;
        void project(float delta_t, const std::vector<float>& qn0, std::vector<float>& qn1,
                     mysh::core::function_view<float(std::size_t idx)> u_solid);

    private:
        float estimateAdvectionDeltaT() const;
        float estimateBodyForcesDeltaT() const;
        float estimateProjectDeltaT() const;

        void presure_gradient_rhs(const std::vector<float>& u, std::vector<float>& rhs, mysh::core::function_view<float(std::size_t idx)> u_solid) const;
        void setup_A(float delta_t);

        float toPosition(std::size_t index) const;
        std::size_t toGrid(float position) const;

        float interpolate(const utils::boundary_span<const float>& q, float x_P) const;
        float integrate(const utils::boundary_span<const float>& f, float q, float delta_t) const;

        const float m_delta_x;
        const float m_g;
        const float m_density;

        float tn0 = 0.0f;
        std::vector<float> m_position;

        std::vector<float> m_p;
        std::vector<float> m_u_n0;
        std::vector<float> m_u_A;
        std::vector<float> m_u_B;
        std::vector<float> m_u_n1;

        std::vector<float> m_rhs;
        std::vector<float> m_A_diag;
        std::vector<float> m_A_x;
    };
}

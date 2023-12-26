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
        [[nodiscard]] float estimateAdvectionDeltaT() const;
        [[nodiscard]] float estimateBodyForcesDeltaT() const;
        [[nodiscard]] float estimateProjectDeltaT() const;

        void presure_gradient_rhs(const std::vector<float>& u, std::vector<float>& rhs, mysh::core::function_view<float(std::size_t idx)> u_solid) const;
        void setup_A(float delta_t);
        void incomplete_cholesky(const std::vector<float>& A_diag, const std::vector<float>& A_x, std::vector<float>& precon) const;
        void preconditioned_conjugate_gradient();
        void apply_preconditioner(const std::vector<float>& A_x, const std::vector<float>& precon,
                                  const std::vector<float>& r, std::vector<float>& z, std::vector<float>& q) const;
        void apply_A(const std::vector<float>& A_diag, const std::vector<float>& A_x, const std::vector<float>& s,
                                std::vector<float>& z) const;
        void presure_gradient_update();

        [[nodiscard]] float toPosition(std::size_t index) const;
        [[nodiscard]] std::size_t toGrid(float position) const;

        [[nodiscard]] float interpolate(const utils::boundary_span<const float>& q, float x_P) const;
        [[nodiscard]] float integrate(const utils::boundary_span<const float>& f, float q, float delta_t) const;

         // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
        const float m_delta_x;
        const float m_g;
        const float m_density;
        // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

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
        std::vector<float> m_precon;

        std::vector<float> m_b;
        std::vector<float> m_r;
        std::vector<float> m_q;
        std::vector<float> m_z;
        std::vector<float> m_s;
    };
}

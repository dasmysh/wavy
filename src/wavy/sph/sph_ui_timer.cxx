/**
 * @file   sph_ui_timer.cxx
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.20
 *
 * @brief  Module implementation of the SPH timer.
 */

module;

#include <glm/common.hpp>

module wavy.sphui:timer;
import :timer;

namespace wavy::sph {
    void sph_timer::update_time(sph_solver& solver, sph_gui& gui, float delta_t,
                                const sph_solver::external_influence* ext_influence)
    {
        m_delta_t_out_of_bounds = false;
        if (delta_t > 1.5f * m_last_delta_t || delta_t < 0.5f * m_last_delta_t) { m_delta_t_out_of_bounds = true; }
        m_last_delta_t = glm::mix(m_last_delta_t, delta_t, .3f);

        float delta_t_step = delta_t / static_cast<float>(gui.get_sim_steps_per_frame());
        for (int i = 0; i < gui.get_sim_steps_per_frame(); ++i) {
            solver.simulation_step(delta_t_step * gui.get_sim_time_scale(), ext_influence);
        }
    }
}

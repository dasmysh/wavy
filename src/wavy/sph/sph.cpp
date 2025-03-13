/**
 * @file   sph.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief  Implementation of the SPH(smooth particle hydrodynamics) solver manager.
 */

#include "main.h"
#include "sph/sph.h"
#include "sph/sph_solver.h"
#include "utils/enumerate.h"

#include "imgui.h"
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <numeric>
#include <execution>

namespace wavy::sph {

    sph::sph(const glm::vec2& sim_area)
        : m_solver{std::make_unique<sph_solver>(sim_area)}
        , m_conversions{sim_area}
        , m_visualization{*m_solver}
    {
        m_ext_influence.radius = DEFAULT_INFLUENCE_RADIUS;
    }

    sph::~sph() = default;

    void sph::simulation_frame(float delta_t)
    {
        m_ext_influence.strength = 0.f;
        if (!m_pause_simulation || m_input.is_space_pressed()) {
            m_ext_influence.position = m_input.get_mouse_pos_simulation();

            if (m_input.is_p_down() && m_input.is_mouse_left_down()) {
                m_ext_influence.strength = 1.f * m_gui.get_ext_influence_strength();
            }
            if (m_input.is_p_down() && m_input.is_mouse_right_down()) {
                m_ext_influence.strength = -1.f * m_gui.get_ext_influence_strength();
            }

            m_timer.update_time(*m_solver, m_gui, delta_t, m_input.is_p_down() ? &m_ext_influence : nullptr);
        }
    }

    void sph::draw_gui()
    {
        m_gui.draw_gui(*m_solver);

        if (m_gui.should_update_smoothing_kernels()) { m_visualization.update_smoothing_kernels(*m_solver); }

        m_input.reset_state();
    }

    void sph::draw_simulation(sf::RenderTarget& rt)
    {
        m_visualization.draw_simulation(*m_solver, m_conversions, m_gui, m_input, m_timer, rt);
    }

    void sph::process_event(const sf::Event& event)
    {
        m_input.process_event(m_conversions, m_gui, event);

        if (!m_pause_simulation && m_input.is_space_pressed()) {
            m_pause_simulation = true;
        }

        if (m_pause_simulation && m_input.is_space_double_pressed()) {
            m_pause_simulation = false;
        }

        if (m_input.is_p_down()) {
            m_ext_influence.radius += m_input.get_mouse_wheel_delta();
        } else {
            m_ext_influence.radius = DEFAULT_INFLUENCE_RADIUS;
        }
    }

}

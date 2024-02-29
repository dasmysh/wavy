/**
 * @file   sph.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief  Implementation of the SPH(smooth particle hydrodynamics) solver manager.
 */

#include "sph/sph.h"
#include "sph/sph_solver.h"

#include "imgui.h"
#include <glm/common.hpp>
#include <spdlog/spdlog.h>
#include <array>

namespace wavy::sph {

    sph::sph(const glm::vec2& sim_area)
        : m_sim_area{ sim_area }
        , m_solver{std::make_unique<sph_solver>(sim_area)}
    {
        auto font_file = "../assets/monaspace/MonaspaceNeonVarVF[wght,wdth,slnt].ttf";
        if (!m_delta_t_font.loadFromFile(font_file)) { spdlog::error("Could not load font: {}", font_file); }


    }

    sph::~sph() = default;

    void sph::simulation_frame(float delta_t)
    {
        m_delta_t_out_of_bounds = false;
        if (delta_t > 1.5f * m_last_delta_t || delta_t < 0.5f * m_last_delta_t) { m_delta_t_out_of_bounds = true; }
        m_last_delta_t = glm::mix(m_last_delta_t, delta_t, .3f);

        float delta_t_step = delta_t / static_cast<float>(m_sim_steps_per_frame);
        for (int i = 0; i < m_sim_steps_per_frame; ++i) {
            m_solver->simulation_step(delta_t_step * m_sim_time_scale);
        }
    }

    void wavy::sph::sph::draw_gui()
    {
        if (ImGui::Begin("SPH Settings")) {

            ImGui::SliderFloat("Visual Particle Radius", &m_visual_particle_radius, .001f, 100.f);
            ImGui::SliderFloat("Simulation Time Scale", &m_sim_time_scale, .1f, 100000.f);
            ImGui::InputInt("Simulation Steps per Frame", &m_sim_steps_per_frame);
            m_sim_steps_per_frame = glm::max(1, m_sim_steps_per_frame);

            ImGui::Separator();

            if (auto particle_count = static_cast<int>(m_solver->get_particles().size());
                ImGui::InputInt("Particle Count", &particle_count))
            {
                m_solver->set_particle_count(static_cast<std::size_t>(particle_count));
            }

            if (auto particle_radius = m_solver->get_particle_radius();
                ImGui::SliderFloat("Particle Radius", &particle_radius, .001f, 100.f)) {
                m_solver->set_particle_radius(particle_radius);
            }

            std::array<const char*, 2> pattern_names{"random", "centered_grid"};
            if (auto current_pattern = static_cast<int>(m_solver->get_pattern());
                ImGui::Combo("Pattern", &current_pattern, pattern_names.data(), static_cast<int>(pattern_names.size()))) {
                m_solver->set_particle_pattern(static_cast<particle_pattern>(current_pattern));
            }

            static int seed = 1337;
            if (m_solver->get_pattern() == particle_pattern::random) { ImGui::InputInt("Seed", &seed, 0); }

            if (ImGui::Button("Reset Particles")) { m_solver->reset_particles(seed); }

            ImGui::Separator();

            if (auto gravity = m_solver->get_gravity(); ImGui::SliderFloat("Gravity", &gravity, .0f, 100.f)) {
                m_solver->set_gravity(gravity);
            }

            if (auto collision_dampening = m_solver->get_collision_dampening();
                ImGui::SliderFloat("Collision Dampening", &collision_dampening, .0f, 1.f)) {
                m_solver->set_collision_dampening(collision_dampening);
            }

            ImGui::End();
        }
    }

    void sph::draw_simulation(sf::RenderTarget& rt) const
    {
        auto rt_size = rt.getSize();
        auto area_offset = glm::vec2{0.05f * static_cast<float>(rt_size.x), 0.95f * static_cast<float>(rt_size.y)};
        auto area_size = glm::vec2{0.9f * static_cast<float>(rt_size.x), 0.9f * static_cast<float>(rt_size.y)};

        sf::RectangleShape line_top{sf::Vector2f(static_cast<float>(rt_size.x), static_cast<float>(rt_size.y) * 0.05f)};
        line_top.setFillColor(sf::Color::White);
        auto line_bottom = line_top;
        line_bottom.setPosition(0.0f, static_cast<float>(rt_size.y) * 0.95f);
        sf::RectangleShape line_left{
            sf::Vector2f(static_cast<float>(rt_size.x) * 0.05f, static_cast<float>(rt_size.y))};
        line_left.setFillColor(sf::Color::White);
        auto line_right = line_left;
        line_right.setPosition(static_cast<float>(rt_size.x) * 0.95f, 0.0f);

        rt.draw(line_top);
        rt.draw(line_bottom);
        rt.draw(line_left);
        rt.draw(line_right);

        sf::CircleShape particleShape{m_visual_particle_radius};
        particleShape.setOrigin(m_visual_particle_radius, m_visual_particle_radius);
        particleShape.setFillColor(sf::Color::Blue);

        for (const auto& particles = m_solver->get_particles(); const auto& particle : particles) {
            auto relative_position = particle.position / m_sim_area;
            auto render_position = area_offset + glm::vec2{1.f, -1.f} * relative_position * area_size;
            particleShape.setPosition(render_position.x, render_position.y);
            rt.draw(particleShape);
        }

        sf::Text delta_t_text(fmt::format("{:.3f}", m_last_delta_t), m_delta_t_font);
        delta_t_text.setFillColor(m_delta_t_out_of_bounds ? sf::Color::Red : sf::Color::Green);
        delta_t_text.setOutlineColor(m_delta_t_out_of_bounds ? sf::Color::Red : sf::Color::Green);
        rt.draw(delta_t_text);
    }

}

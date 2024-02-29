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
#include <SFML/Graphics.hpp>
#include <array>

namespace wavy::sph {

    sph::sph(const glm::vec2& sim_area)
        : m_sim_area{ sim_area }
        , m_solver{std::make_unique<sph_solver>(sim_area)}
    {}

    sph::~sph() = default;

    void wavy::sph::sph::draw(sf::RenderTarget& rt)
    {
        auto rt_size = rt.getSize();
        auto area_offset = glm::vec2{0.05f * static_cast<float>(rt_size.x), 0.05f * static_cast<float>(rt_size.y)};
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
            auto render_position = area_offset + relative_position * area_size;
            particleShape.setPosition(render_position.x, render_position.y);
            rt.draw(particleShape);
        }

        if (ImGui::Begin("SPH Settings")) {

            ImGui::SliderFloat("Visual Particle Radius", &m_visual_particle_radius, .001f, 100.f);

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

            ImGui::End();
        }
    }

}

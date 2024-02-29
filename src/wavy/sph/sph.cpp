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

namespace wavy::sph {

    sph::sph(const glm::vec2& sim_area)
        : m_sim_area{sim_area}
        , m_solver{std::make_unique<sph_solver>(sim_area)}
    {
        if (auto font_file = "../assets/monaspace/MonaspaceNeonVarVF[wght,wdth,slnt].ttf";
            !m_delta_t_font.loadFromFile(font_file)) {
            spdlog::error("Could not load font: {}", font_file);
        }

        update_smoothing_kernels();
    }

    sph::~sph() = default;

    void sph::simulation_frame(float delta_t)
    {
        m_delta_t_out_of_bounds = false;
        if (delta_t > 1.5f * m_last_delta_t || delta_t < 0.5f * m_last_delta_t) { m_delta_t_out_of_bounds = true; }
        m_last_delta_t = glm::mix(m_last_delta_t, delta_t, .3f);

        float delta_t_step = delta_t / static_cast<float>(m_sim_steps_per_frame);
        for (int i = 0; i < m_sim_steps_per_frame; ++i) { m_solver->simulation_step(delta_t_step * m_sim_time_scale); }
    }

    void wavy::sph::sph::draw_gui()
    {
        if (ImGui::Begin("SPH Settings")) {

            ImGui::SliderFloat("Visual Particle Radius", &m_visual_particle_radius, .001f, 100.f);
            ImGui::SliderFloat("Simulation Time Scale", &m_sim_time_scale, .1f, 100000.f);
            ImGui::InputInt("Simulation Steps per Frame", &m_sim_steps_per_frame);
            m_sim_steps_per_frame = glm::max(1, m_sim_steps_per_frame);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (auto particle_count = static_cast<int>(m_solver->get_particles().size());
                ImGui::InputInt("Particle Count", &particle_count)) {
                m_solver->set_particle_count(static_cast<std::size_t>(particle_count));
                m_update_scalar_field = true;
            }

            std::array<const char*, 2> pattern_names{"random", "centered_grid"};
            if (auto current_pattern = static_cast<int>(m_solver->get_pattern()); ImGui::Combo(
                    "Pattern", &current_pattern, pattern_names.data(), static_cast<int>(pattern_names.size()))) {
                m_solver->set_particle_pattern(static_cast<particle_pattern>(current_pattern));
                m_update_scalar_field = true;
            }

            static int seed = 1337;
            if (m_solver->get_pattern() == particle_pattern::random) { ImGui::InputInt("Seed", &seed, 0); }

            if (ImGui::Button("Reset Particles")) {
                m_solver->reset_particles(seed);
                m_update_scalar_field = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (auto particle_radius = m_solver->get_particle_radius();
                ImGui::SliderFloat("Particle Radius", &particle_radius, .001f, 100.f)) {
                m_solver->set_particle_radius(particle_radius);
                update_smoothing_kernels();
                m_update_scalar_field = true;
            }

            if (auto particle_mass = m_solver->get_particle_mass();
                ImGui::SliderFloat("Particle Mass", &particle_mass, .001f, 100.f)) {
                m_solver->set_particle_mass(particle_mass);
                m_update_scalar_field = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (auto gravity = m_solver->get_gravity(); ImGui::SliderFloat("Gravity", &gravity, .0f, 100.f)) {
                m_solver->set_gravity(gravity);
            }

            if (auto collision_dampening = m_solver->get_collision_dampening();
                ImGui::SliderFloat("Collision Dampening", &collision_dampening, .0f, 1.f)) {
                m_solver->set_collision_dampening(collision_dampening);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (bool visualize_scalar = m_visualize_scalar != -1;
                ImGui::Checkbox("Visualize Density Field", &visualize_scalar)) {
                m_visualize_scalar = visualize_scalar ? 0 : -1;
            }

            if (m_visualize_scalar != -1) {
                ImGui::Checkbox("As Field", &m_show_scalar_field_texture);
                m_update_scalar_field |= ImGui::RadioButton("Density", &m_visualize_scalar, 0);
                m_update_scalar_field |= ImGui::RadioButton("Dummy Property", &m_visualize_scalar, 1);
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

        if (m_visualize_scalar != -1) {
            if (m_show_scalar_field_texture) {
                visualize_scalar_field(rt, static_cast<std::size_t>(m_visualize_scalar), area_offset, area_size);
            } else {
                visualize_scalar_field_points(rt, static_cast<std::size_t>(m_visualize_scalar), area_offset, area_size);
            }
        }

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

    void sph::visualize_scalar_field_points(sf::RenderTarget& rt, std::size_t i, const glm::vec2& area_offset,
                                            const glm::vec2& area_size) const
    {
        assert(i < m_smoothing_kernels.size());

        auto radius = glm::ceil(m_solver->get_particle_radius());

        sf::Sprite kernel_sprite;
        kernel_sprite.setTexture(m_smoothing_kernels[i]);
        kernel_sprite.setOrigin(radius, radius);
        kernel_sprite.setColor(sf::Color::Blue);

        sf::BlendMode accumulate_blending{sf::BlendMode::SrcAlpha, sf::BlendMode::One, sf::BlendMode::Add};

        for (const auto& particles = m_solver->get_particles(); const auto& particle : particles) {
            auto relative_position = particle.position / m_sim_area;
            auto render_position = area_offset + glm::vec2{1.f, -1.f} * relative_position * area_size;
            kernel_sprite.setPosition(render_position.x, render_position.y);
            sf::Uint8 v = 255;
            if (i == 1) {
                float property = particle.property;
                float scaled_prop = 512.f * (.5f * property + .5f);
                v = static_cast<sf::Uint8>(glm::clamp(scaled_prop, 0.f, 255.f));
            }
            kernel_sprite.setColor(sf::Color{v, v, v, v} * sf::Color::Blue);
            rt.draw(kernel_sprite, accumulate_blending);
        }
    }

    void sph::visualize_scalar_field(sf::RenderTarget& rt, std::size_t i, const glm::vec2& area_offset,
                                     const glm::vec2& area_size) const
    {
        if (m_update_scalar_field || m_scalar_field_texture.getSize() != rt.getSize()) {
            update_scalar_field_texture(rt, i, area_offset, area_size);
            m_update_scalar_field = false;
        }

        sf::Sprite field_sprite;
        field_sprite.setTexture(m_scalar_field_texture);
        rt.draw(field_sprite);
    }

    void sph::update_smoothing_kernels()
    {
        auto radius = static_cast<unsigned int>(glm::ceil(m_solver->get_particle_radius()));
        for (std::size_t i = 0; i < m_smoothing_kernels.size(); ++i) { update_smoothing_kernel(i, radius); }
    }

    void sph::update_smoothing_kernel(std::size_t i, unsigned int radius)
    {
        auto& kernel = m_smoothing_kernels[i];

        sf::Image kernel_image;
        kernel_image.create(2 * radius, 2 * radius);

        float scale = 0.f;
        if (i == 0) { scale = .2f / m_solver->density_kernel(0.f); }
        if (i == 1) { scale = .5f / m_solver->property_kernel(0.f); }
        for (unsigned int iy = 0; iy < radius; ++iy) {
            for (unsigned int ix = 0; ix < radius; ++ix) {
                float value = 0.f;

                glm::vec2 p = glm::vec2{ix, iy} + glm::vec2{0.5};
                glm::vec2 delta = p - glm::vec2{static_cast<float>(radius)};
                float r = glm::length(delta);

                if (i == 0) { value = m_solver->density_kernel(r) * scale; }
                if (i == 1) { value = m_solver->property_kernel(r) * scale; }

                auto v = static_cast<sf::Uint8>(value * 255.f);
                auto c = sf::Color(255, 255, 255, v);
                kernel_image.setPixel(ix, iy, c);
                kernel_image.setPixel(2 * radius - ix - 1, iy, c);
                kernel_image.setPixel(ix, 2 * radius - iy - 1, c);
                kernel_image.setPixel(2 * radius - ix - 1, 2 * radius - iy - 1, c);
            }
        }

        kernel.loadFromImage(kernel_image);
        kernel.setSmooth(true);
    }

    void sph::update_scalar_field_texture(const sf::RenderTarget& rt, std::size_t i, const glm::vec2& area_offset,
                                          const glm::vec2& area_size) const
    {
        sf::Image scalar_field_image;
        scalar_field_image.create(rt.getSize().x, rt.getSize().y);

        float scale = 0.f;
        if (i == 0) { scale = .2f / m_solver->density_kernel(0.f); }
        if (i == 1) { scale = .5f / m_solver->property_kernel(0.f); }

        auto scalar_field_size = rt.getSize();
        for (auto iy = 0u; iy < scalar_field_size.y; ++iy) {
            for (auto ix = 0u; ix < scalar_field_size.x; ++ix) {
                glm::vec2 p = glm::vec2{ix, iy} + glm::vec2{0.5};

                auto relative_position = glm::vec2{1.f, -1.f} * (p - area_offset) / area_size;
                auto sim_position = relative_position * m_sim_area;

                auto c = calculate_scalar_color_at(sim_position, i, scale);
                scalar_field_image.setPixel(ix, iy, c);
            }
        }

        m_scalar_field_texture.loadFromImage(scalar_field_image);
        m_scalar_field_texture.setSmooth(true);
    }

    sf::Color sph::calculate_scalar_color_at(const glm::vec2& sim_position, std::size_t i, float scale) const
    {
        auto c = sf::Color{0, 0, 0, 0};
        if (sim_position.x >= 0.f && sim_position.x < m_sim_area.x && sim_position.y >= 0.f
            && sim_position.y < m_sim_area.y) {
            float value = 0.f;
            if (i == 0) {
                value = m_solver->calculate_density(sim_position);
            } else if (i == 1) {
                value = m_solver->calculate_property(sim_position);
            }

            auto v = static_cast<sf::Uint8>(255.f * glm::clamp(value * scale, 0.f, 1.f));
            c = sf::Color{0, 0, v};
        }
        return c;
    }

}

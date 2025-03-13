/**
 * @file   sph_ui_visualization.cxx
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.07
 *
 * @brief  Module implementation of the SPH visualization
 */

module;

#include "main.h"
#include "utils/enumerate.h"
#include <SFML/Graphics.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/norm.hpp>
#include <numeric>
#include <execution>

module wavy.sphui:visualization;
import :visualization;

namespace wavy::sph {
    sph_visualization::sph_visualization(sph_solver& solver)
    {
        if (auto font_file = "../assets/monaspace/MonaspaceNeonVarVF[wght,wdth,slnt].ttf";
            !m_delta_t_font.loadFromFile(font_file)) {
            spdlog::error("Could not load font: {}", font_file);
        }

        update_smoothing_kernels(solver);
    }

    void sph_visualization::draw_simulation(sph_solver& solver, sph_conversions& conversions, sph_gui& gui,
                                            sph_input& input, sph_timer& timer, sf::RenderTarget& rt)
    {
        conversions.update_metrics(glm::vec2{rt.getSize().x, rt.getSize().y});
        auto border_thickness = conversions.render_area_to_screen(glm::vec2{0.f});

        sf::RectangleShape line_top{sf::Vector2f(conversions.get_screen_size().x, border_thickness.y)};
        line_top.setFillColor(sf::Color::White);
        auto line_bottom = line_top;
        line_bottom.setPosition(0.0f, border_thickness.y + conversions.get_render_size().y);
        sf::RectangleShape line_left{sf::Vector2f(border_thickness.x, conversions.get_screen_size().y)};
        line_left.setFillColor(sf::Color::White);
        auto line_right = line_left;
        line_right.setPosition(border_thickness.x + conversions.get_render_size().x, 0.0f);

        rt.draw(line_top);
        rt.draw(line_bottom);
        rt.draw(line_left);
        rt.draw(line_right);

        draw_grid(solver, conversions, gui, rt);

        if (gui.get_visualize_scalar() != -1) {
            if (gui.is_show_scalar_field_texture()) {
                auto visualize_scalar = static_cast<std::size_t>(gui.get_visualize_scalar());
                if (gui.should_update_scalar_field() || m_scalar_field_texture.getSize() != rt.getSize()) {
                    update_scalar_field_texture(solver, conversions, visualize_scalar);
                }
                visualize_scalar_field(rt);
            } else {
                visualize_scalar_field_points(solver, conversions, rt,
                                              static_cast<std::size_t>(gui.get_visualize_scalar()));
            }
        }

        sf::CircleShape particleShape{gui.get_visual_particle_radius()};
        particleShape.setOrigin(gui.get_visual_particle_radius(), gui.get_visual_particle_radius());
        auto selectedParticleShape = particleShape;
        particleShape.setFillColor(sf::Color::Blue);
        selectedParticleShape.setOutlineThickness(1.f);
        selectedParticleShape.setOutlineColor(sf::Color::Red);
        selectedParticleShape.setFillColor(sf::Color::Blue);

        const glm::vec3 colorTarget{.0f, 1.f, .0f};
        const glm::vec3 colorLow{.0f, .5f, 1.f};
        const glm::vec3 colorHigh{1.f, .5f, .0f};
        for (const auto& particles = solver.get_particles();
             const auto& [index, particle] : utils::enumerate(particles)) {
            auto render_position = conversions.simulation_to_screen(particle.position);
            auto density = particle.density - solver.get_config().get_target_density();
            glm::vec3 particleColor;
            if (density < 0) {
                if (density < -solver.get_config().get_target_density()) {
                    density = 1.f;
                } else {
                    density = density / -solver.get_config().get_target_density();
                }

                particleColor = colorTarget * (1.f - density) + colorLow * density;
            } else {
                if (density > solver.get_config().get_target_density()) {
                    density = 1.f;
                } else {
                    density = density / solver.get_config().get_target_density();
                }

                particleColor = colorTarget * (1.f - density) + colorHigh * density;
            }

            if (input.is_mouse_clicked()
                && glm::distance2(render_position, input.get_mouse_pos_screen())
                       < 4.f * gui.get_visual_particle_radius() * gui.get_visual_particle_radius()) {
                gui.select_particle(index);
            }
            if (gui.get_selected_particle_index() == index) {
                selectedParticleShape.setFillColor(sf::Color{static_cast<uint8_t>(particleColor.r * 255.f),
                                                             static_cast<uint8_t>(particleColor.g * 255.f),
                                                             static_cast<uint8_t>(particleColor.b * 255.f)});
                selectedParticleShape.setPosition(render_position.x, render_position.y);
                rt.draw(selectedParticleShape);
            } else {
                particleShape.setFillColor(sf::Color{static_cast<uint8_t>(particleColor.r * 255.f),
                                                             static_cast<uint8_t>(particleColor.g * 255.f),
                                                             static_cast<uint8_t>(particleColor.b * 255.f)});
                particleShape.setPosition(render_position.x, render_position.y);
                rt.draw(particleShape);
            }
        }

        // draw external influence
        if (auto ext_influence = solver.get_current_external_influence(); ext_influence) {
            auto render_position = conversions.simulation_to_screen(ext_influence->position);
            auto render_origin = conversions.simulation_to_render_area(glm::vec2{ext_influence->radius});
            sf::CircleShape extInfluenceShape{render_origin.x};
            extInfluenceShape.setOrigin(render_origin.x, render_origin.x);
            extInfluenceShape.setPosition(render_position.x, render_position.y);
            extInfluenceShape.setOutlineThickness(1.f);
            extInfluenceShape.setOutlineColor(sf::Color::Red);
            extInfluenceShape.setFillColor(sf::Color::Transparent);
            rt.draw(extInfluenceShape);
        }

        sf::Text delta_t_text(fmt::format("{:.3f}", timer.get_delta_t()), m_delta_t_font);
        delta_t_text.setFillColor(timer.is_delta_t_out_of_bounds() ? sf::Color::Red : sf::Color::Green);
        delta_t_text.setOutlineColor(timer.is_delta_t_out_of_bounds() ? sf::Color::Red : sf::Color::Green);
        rt.draw(delta_t_text);
    }

    void sph_visualization::visualize_scalar_field_points(sph_solver& solver, sph_conversions& conversions,
                                                          sf::RenderTarget& rt, std::size_t i) const
    {
        assert(i < m_smoothing_kernels.size());

        auto radius = glm::ceil(solver.get_config().get_particle_radius());

        sf::Sprite kernel_sprite;
        kernel_sprite.setTexture(m_smoothing_kernels[i]);
        kernel_sprite.setOrigin(radius, radius);
        kernel_sprite.setColor(sf::Color::Blue);

        sf::BlendMode accumulate_blending{sf::BlendMode::SrcAlpha, sf::BlendMode::One, sf::BlendMode::Add};

        for (const auto& particles = solver.get_particles(); const auto& particle : particles) {
            auto render_position = conversions.simulation_to_screen(particle.position);
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

    void sph_visualization::visualize_scalar_field(sf::RenderTarget& rt) const
    {
        sf::Sprite field_sprite;
        field_sprite.setTexture(m_scalar_field_texture);
        rt.draw(field_sprite);
    }

    void sph_visualization::draw_grid(sph_solver& solver, sph_conversions& conversions, sph_gui& gui,
                                      sf::RenderTarget& rt) const
    {
        constexpr float line_thickness = 3.f;
        auto grid_size = conversions.simulation_to_render_area(
            glm::vec2{solver.get_config().get_particle_radius(),
                      conversions.get_simulation_size().y - solver.get_config().get_particle_radius()});
        sf::Color line_color{150, 150, 150, 100};

        sf::RectangleShape line_horizontal{sf::Vector2f{conversions.get_render_size().x, line_thickness}};
        line_horizontal.setOrigin(-conversions.render_area_to_screen(glm::vec2{0.f}).x, .5f * line_thickness);
        line_horizontal.setOutlineColor(line_color);
        line_horizontal.setFillColor(line_color);

        sf::RectangleShape line_vertical{sf::Vector2f{line_thickness, conversions.get_render_size().y}};
        line_vertical.setOrigin(.5f * line_thickness, -conversions.render_area_to_screen(glm::vec2{0.f}).y);
        line_vertical.setOutlineColor(line_color);
        line_vertical.setFillColor(line_color);

        const auto line_start = conversions.render_area_to_screen(
            glm::abs(conversions.simulation_to_render_area(glm::vec2{solver.get_config().get_particle_radius()})));
        const auto line_end = conversions.render_area_to_screen(glm::vec2{conversions.get_render_size().x, 0.f});

        float y_line = line_start.y;
        while (y_line > line_end.y) {
            line_horizontal.setPosition(0.f, y_line);
            rt.draw(line_horizontal);
            y_line -= grid_size.y;
        }

        float x_line = line_start.x;
        while (x_line < line_end.x) {
            line_vertical.setPosition(x_line, 0.f);
            rt.draw(line_vertical);
            x_line += grid_size.x;
        }

        sf::RectangleShape selected_cell_highlight{
            sf::Vector2f{grid_size.x - 2.f * line_thickness, grid_size.y - 2.f * line_thickness}};
        selected_cell_highlight.setOutlineColor(sf::Color::Red);
        selected_cell_highlight.setOutlineThickness(line_thickness);
        selected_cell_highlight.setFillColor(sf::Color::Transparent);
        selected_cell_highlight.setOrigin(-line_thickness, grid_size.y - line_thickness);

        auto screen_view = rt.getView();
        auto render_area_view = screen_view;
        render_area_view.setSize(conversions.get_render_size().x, conversions.get_render_size().y);
        render_area_view.setViewport(sf::FloatRect{.05f, .05f, .9f, .9f});
        rt.setView(render_area_view);

        auto start_cell = solver.grid_cell(glm::vec2{0.f});
        auto end_cell = solver.grid_cell(conversions.get_simulation_size());

        for (int iy = start_cell.y; iy <= end_cell.y; ++iy) {
            for (int ix = start_cell.x; ix <= end_cell.x; ++ix) {
                auto cell_hash = sph_solver::grid_hash(glm::ivec2{ix, iy});
                if (gui.get_selected_cell_hash() == cell_hash) {
                    glm::vec2 simulation_position{static_cast<float>(ix) * solver.get_config().get_particle_radius(),
                                                  static_cast<float>(iy)
                                                      * solver.get_config().get_particle_radius()};
                    auto screen_position = conversions.simulation_to_screen(simulation_position);
                    selected_cell_highlight.setPosition(screen_position.x, screen_position.y);
                    rt.draw(selected_cell_highlight);
                }
            }
        }

        rt.setView(screen_view);
    }

    void sph_visualization::update_scalar_field_texture(sph_solver& solver, sph_conversions& conversions,
                                                        std::size_t i) const
    {
        sf::Image scalar_field_image;
        scalar_field_image.create(static_cast<unsigned int>(conversions.get_screen_size().x),
                                  static_cast<unsigned int>(conversions.get_screen_size().y));

        if (m_screen_ys.size() != static_cast<std::size_t>(conversions.get_screen_size().y)) {
            m_screen_ys.resize(static_cast<std::size_t>(conversions.get_screen_size().y));
            std::ranges::iota(m_screen_ys, 0);
        }

        float scale = 0.f;
        if (i == 0) { scale = .2f / solver.density_kernel(0.f); }
        if (i == 1) { scale = .5f / solver.pressure_kernel(0.f); }

        std::for_each(std::execution::par, std::begin(m_screen_ys), std::end(m_screen_ys),
                      [this, &solver, &conversions, scale, &scalar_field_image, i](auto iy) {
                          for (auto ix = 0u; ix < static_cast<unsigned int>(conversions.get_screen_size().x); ++ix) {
                              glm::vec2 p = glm::vec2{ix, iy} + glm::vec2{0.5};
                              auto simulation_position = conversions.screen_to_simulation(p);

                              auto c = calculate_scalar_color_at(solver, conversions, simulation_position, i, scale);
                              scalar_field_image.setPixel(ix, iy, c);
                          }
                      });

        m_scalar_field_texture.setSrgb(false);
        m_scalar_field_texture.loadFromImage(scalar_field_image);
        m_scalar_field_texture.setSmooth(true);
    }

    void sph_visualization::update_smoothing_kernels(sph_solver& solver)
    {
        auto radius = static_cast<unsigned int>(glm::ceil(solver.get_config().get_particle_radius()));
        for (std::size_t i = 0; i < m_smoothing_kernels.size(); ++i) { update_smoothing_kernel(solver, i, radius); }
    }

    void sph_visualization::update_smoothing_kernel(sph_solver& solver, std::size_t i, unsigned int radius)
    {
        auto& kernel = m_smoothing_kernels[i];

        sf::Image kernel_image;
        kernel_image.create(2 * radius, 2 * radius);

        float scale = 0.f;
        if (i == 0) { scale = .2f / solver.density_kernel(0.f); }
        if (i == 1) { scale = .5f / solver.pressure_kernel(0.f); }
        for (unsigned int iy = 0; iy < radius; ++iy) {
            for (unsigned int ix = 0; ix < radius; ++ix) {
                float value = 0.f;

                glm::vec2 p = glm::vec2{ix, iy} + glm::vec2{0.5};
                glm::vec2 delta = p - glm::vec2{static_cast<float>(radius)};
                float r = glm::length(delta);

                if (i == 0) { value = solver.density_kernel(r) * scale; }
                if (i == 1) { value = solver.pressure_kernel(r) * scale; }

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

    sf::Color sph_visualization::calculate_scalar_color_at(sph_solver& solver, sph_conversions& conversions,
                                                           const glm::vec2& sim_position, std::size_t i,
                                                           float scale) const
    {
        auto c = sf::Color{0, 0, 0, 0};
        if (sim_position.x >= 0.f && sim_position.x < conversions.get_simulation_size().x && sim_position.y >= 0.f
            && sim_position.y < conversions.get_simulation_size().y) {
            float value = 0.f;
            if (i == 0) {
                value = solver.calculate_density(sim_position);
            } else if (i == 1) {
                value = solver.calculate_property(sim_position);
            }

            auto v = static_cast<sf::Uint8>(255.f * glm::clamp(value * scale, 0.f, 1.f));
            c = sf::Color{0, 0, 255, v};
        }
        return c;
    }
}

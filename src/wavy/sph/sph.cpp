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
#include "imgui_stdlib.h"
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <numeric>
#include <execution>

template<> struct std::hash<glm::uvec2>
{
    std::size_t operator()(const glm::uvec2& k) const
    {
        return wavy::sph::sph_solver::grid_hash(k);
    }
};

namespace wavy::sph {

    sph::sph(const glm::vec2& sim_area)
        : m_simulation_size{sim_area}
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
        if (ImGui::Begin("SPH")) {
            if (ImGui::BeginTabBar("SPH#tabs_bar")) {
                draw_settings_gui();
                draw_particle_info_gui();
                draw_cell_info_gui();
                ImGui::EndTabBar();
            }

            ImGui::End();
        }
    }

    void sph::draw_simulation(sf::RenderTarget& rt)
    {
        constexpr float border_size_ratio = .05f;
        constexpr float render_area_size_ratio = 1.f - 2.f * border_size_ratio;
        m_screen_size = glm::vec2{rt.getSize().x, rt.getSize().y};
        m_render_offset = glm::vec2{border_size_ratio * m_screen_size.x, border_size_ratio * m_screen_size.y};
        m_render_size = glm::vec2{render_area_size_ratio * m_screen_size.x, render_area_size_ratio * m_screen_size.y};

        sf::RectangleShape line_top{sf::Vector2f(m_screen_size.x, m_screen_size.y * border_size_ratio)};
        line_top.setFillColor(sf::Color::White);
        auto line_bottom = line_top;
        line_bottom.setPosition(0.0f, m_screen_size.y * border_size_ratio + m_render_size.y);
        sf::RectangleShape line_left{sf::Vector2f(m_screen_size.x * border_size_ratio, m_screen_size.y)};
        line_left.setFillColor(sf::Color::White);
        auto line_right = line_left;
        line_right.setPosition(m_screen_size.x * border_size_ratio + m_render_size.x, 0.0f);

        rt.draw(line_top);
        rt.draw(line_bottom);
        rt.draw(line_left);
        rt.draw(line_right);

        draw_grid(rt);

        if (m_visualize_scalar != -1) {
            if (m_show_scalar_field_texture) {
                visualize_scalar_field(rt, static_cast<std::size_t>(m_visualize_scalar));
            } else {
                visualize_scalar_field_points(rt, static_cast<std::size_t>(m_visualize_scalar));
            }
        }

        sf::CircleShape particleShape{m_visual_particle_radius};
        particleShape.setOrigin(m_visual_particle_radius, m_visual_particle_radius);
        auto selectedParticleShape = particleShape;
        particleShape.setFillColor(sf::Color::Blue);
        selectedParticleShape.setOutlineThickness(1.f);
        selectedParticleShape.setOutlineColor(sf::Color::Red);
        selectedParticleShape.setFillColor(sf::Color::Blue);

        for (const auto& particles = m_solver->get_particles(); const auto& [index, particle] : utils::enumerate(particles)) {
            auto render_position = simulation_to_screen(particle.position);
            if (m_mouse_clicked
                && glm::distance2(particle.position, m_mouse_pos_simulation)
                       < m_visual_particle_radius * m_visual_particle_radius) {
                m_selected_particle_index = index;
            }
            if (m_selected_particle_index == index) {
                selectedParticleShape.setPosition(render_position.x, render_position.y);
                rt.draw(selectedParticleShape);
            } else {
                particleShape.setPosition(render_position.x, render_position.y);
                rt.draw(particleShape);
            }
        }

        sf::Text delta_t_text(fmt::format("{:.3f}", m_last_delta_t), m_delta_t_font);
        delta_t_text.setFillColor(m_delta_t_out_of_bounds ? sf::Color::Red : sf::Color::Green);
        delta_t_text.setOutlineColor(m_delta_t_out_of_bounds ? sf::Color::Red : sf::Color::Green);
        rt.draw(delta_t_text);
    }

    void sph::process_event(const sf::Event& event)
    {
        m_mouse_clicked = false;
        if (const auto& io = ImGui::GetIO(); !io.WantCaptureMouse) {
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                glm::vec2 mouse_pos_screen{static_cast<float>(event.mouseButton.x),
                                           static_cast<float>(event.mouseButton.y)};
                m_mouse_pos_simulation = screen_to_simulation(mouse_pos_screen);
                m_mouse_clicked = true;
                m_selected_particle_index = static_cast<std::size_t>(-1);
            }

            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right) {
                m_selected_particle_index = static_cast<std::size_t>(-1);
            }
        }
    }

    void sph::visualize_scalar_field_points(sf::RenderTarget& rt, std::size_t i) const
    {
        assert(i < m_smoothing_kernels.size());

        auto radius = glm::ceil(m_solver->get_particle_radius());

        sf::Sprite kernel_sprite;
        kernel_sprite.setTexture(m_smoothing_kernels[i]);
        kernel_sprite.setOrigin(radius, radius);
        kernel_sprite.setColor(sf::Color::Blue);

        sf::BlendMode accumulate_blending{sf::BlendMode::SrcAlpha, sf::BlendMode::One, sf::BlendMode::Add};

        for (const auto& particles = m_solver->get_particles(); const auto& particle : particles) {
            auto render_position = simulation_to_screen(particle.position);
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

    void sph::visualize_scalar_field(sf::RenderTarget& rt, std::size_t i) const
    {
        if (m_update_scalar_field || m_scalar_field_texture.getSize() != rt.getSize()) {
            update_scalar_field_texture(i);
            m_update_scalar_field = false;
        }

        sf::Sprite field_sprite;
        field_sprite.setTexture(m_scalar_field_texture);
        rt.draw(field_sprite);
    }

    void sph::draw_grid(sf::RenderTarget& rt) const
    {
        constexpr float line_thickness = 3.f;
        auto grid_size = simulation_to_render_area(glm::vec2{m_solver->get_particle_radius(), m_simulation_size.y - m_solver->get_particle_radius()});
        sf::Color line_color{150, 150, 150, 100};

        sf::RectangleShape line_horizontal{sf::Vector2f(m_render_size.x, line_thickness)};
        line_horizontal.setOrigin(-render_area_to_screen(glm::vec2{0.f}).x, .5f * line_thickness);
        line_horizontal.setOutlineColor(line_color);
        line_horizontal.setFillColor(line_color);

        sf::RectangleShape line_vertical{sf::Vector2f(line_thickness, m_render_size.y)};
        line_vertical.setOrigin(.5f * line_thickness,
                                -render_area_to_screen(glm::vec2{0.f}).y);
        line_vertical.setOutlineColor(line_color);
        line_vertical.setFillColor(line_color);

        const auto line_start = render_area_to_screen(glm::abs(simulation_to_render_area(glm::vec2{m_solver->get_particle_radius()})));
        const auto line_end = render_area_to_screen(glm::vec2{m_render_size.x, 0.f});

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

    void sph::update_scalar_field_texture(std::size_t i) const
    {
        sf::Image scalar_field_image;
        scalar_field_image.create(static_cast<unsigned int>(m_screen_size.x),
                                  static_cast<unsigned int>(m_screen_size.y));

        if (m_screen_ys.size() != static_cast<std::size_t>(m_screen_size.y)) {
            m_screen_ys.resize(static_cast<std::size_t>(m_screen_size.y));
            std::ranges::iota(m_screen_ys, 0);
        }

        float scale = 0.f;
        if (i == 0) { scale = .2f / m_solver->density_kernel(0.f); }
        if (i == 1) { scale = .5f / m_solver->property_kernel(0.f); }


        std::for_each(std::execution::par, std::begin(m_screen_ys), std::end(m_screen_ys),
                      [this, scale, &scalar_field_image, i](auto iy) {
                          for (auto ix = 0u; ix < static_cast<unsigned int>(m_screen_size.x); ++ix) {
                              glm::vec2 p = glm::vec2{ix, iy} + glm::vec2{0.5};
                              auto simulation_position = screen_to_simulation(p);

                              auto c = calculate_scalar_color_at(simulation_position, i, scale);
                              scalar_field_image.setPixel(ix, iy, c);
                          }
                      });

        m_scalar_field_texture.setSrgb(false);
        m_scalar_field_texture.loadFromImage(scalar_field_image);
        m_scalar_field_texture.setSmooth(true);
    }

    sf::Color sph::calculate_scalar_color_at(const glm::vec2& sim_position, std::size_t i, float scale) const
    {
        auto c = sf::Color{0, 0, 0, 0};
        if (sim_position.x >= 0.f && sim_position.x < m_simulation_size.x && sim_position.y >= 0.f
            && sim_position.y < m_simulation_size.y) {
            float value = 0.f;
            if (i == 0) {
                value = m_solver->calculate_density(sim_position);
            } else if (i == 1) {
                value = m_solver->calculate_property(sim_position);
            }

            auto v = static_cast<sf::Uint8>(255.f * glm::clamp(value * scale, 0.f, 1.f));
            c = sf::Color{0, 0, 255, v};
        }
        return c;
    }

    void sph::draw_settings_gui()
    {
        if (ImGui::BeginTabItem("Settings")) {
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
                m_selected_particle_index = static_cast<std::size_t>(-1);
            }

            std::array<const char*, 2> pattern_names{"random", "centered_grid"};
            if (auto current_pattern = static_cast<int>(m_solver->get_pattern()); ImGui::Combo(
                    "Pattern", &current_pattern, pattern_names.data(), static_cast<int>(pattern_names.size()))) {
                m_solver->set_particle_pattern(static_cast<particle_pattern>(current_pattern));
                m_update_scalar_field = true;
                m_selected_particle_index = static_cast<std::size_t>(-1);
            }

            static int seed = 1337;
            if (m_solver->get_pattern() == particle_pattern::random) { ImGui::InputInt("Seed", &seed, 0); }

            if (ImGui::Button("Reset Particles")) {
                m_solver->reset_particles(seed);
                m_update_scalar_field = true;
                m_selected_particle_index = static_cast<std::size_t>(-1);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (auto particle_radius = m_solver->get_particle_radius();
                ImGui::SliderFloat("Particle Radius", &particle_radius, .001f, 500.f)) {
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
            ImGui::EndTabItem();
        }
    }

    void sph::draw_particle_info_gui()
    {
        if (ImGui::BeginTabItem("Particle Info")) {
            if (ImGui::BeginTable("particle_props", 6,
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Position");
                ImGui::TableSetupColumn("Velocity");
                ImGui::TableSetupColumn("Density");
                ImGui::TableSetupColumn("Property");
                ImGui::TableSetupColumn("Grid Cell");
                ImGui::TableSetupColumn("Grid Index");
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                for (const auto& [index, particle] : utils::enumerate(m_solver->get_particles())) {
                    draw_particle_info_table_rows(index, particle);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
    }

    void sph::draw_particle_info_table_rows(std::size_t index, const sph_solver::particle& particle)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Selectable(fmt::format("({}, {})", particle.position.x, particle.position.y).c_str(),
                              index == m_selected_particle_index,
                              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
            m_selected_particle_index = index;
        }
        ImGui::TableNextColumn();
        ImGui::Text("(%f, %f)", particle.velocity.x, particle.velocity.y);
        ImGui::TableNextColumn();
        ImGui::Text("%f", particle.density);
        ImGui::TableNextColumn();
        ImGui::Text("%f", particle.property);
        ImGui::TableNextColumn();
        auto grid_cell = m_solver->grid_cell(particle.position);
        ImGui::Text("(%u, %u)", grid_cell.x, grid_cell.y);
        ImGui::TableNextColumn();
        ImGui::Text("%uz", particle.grid_index);
    }

    void sph::draw_cell_info_gui()
    {
        if (ImGui::BeginTabItem("Cell Info")) {
            if (ImGui::BeginTable("cell_props", 7,
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY
                                      | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Grid Index");
                ImGui::TableSetupColumn("Particle Count");
                ImGui::TableSetupColumn("Grid Cells");
                ImGui::TableSetupColumn("Particle Position");
                ImGui::TableSetupColumn("Particle Velocity");
                ImGui::TableSetupColumn("Particle Density");
                ImGui::TableSetupColumn("Particle Property");
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                for (const auto& [index, cell] : utils::enumerate(m_solver->get_cell_sizes())) {
                    draw_cell_info_table_rows(index, cell);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
    }

    void sph::draw_cell_info_table_rows(std::size_t index, const std::atomic_int& cell_size_atomic)
    {
        std::unordered_map<glm::uvec2, std::vector<std::size_t>> grid_cell_to_particle_indices;
        auto cell_offset = m_solver->get_cell_offsets()[index];
        auto cell_size = static_cast<std::size_t>(cell_size_atomic.load());
        auto cell_end = cell_offset + cell_size;
        for (std::size_t particle_index_i = cell_offset; particle_index_i < cell_end; ++particle_index_i) {
            auto particle_index = m_solver->get_particle_indices()[particle_index_i];
            const auto& particle = m_solver->get_particles()[particle_index];
            grid_cell_to_particle_indices[m_solver->grid_cell(particle.position)].push_back(particle_index);
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%uz", index);
        ImGui::TableNextColumn();
        bool list_cells = false;
        if (cell_size != 0) {
            list_cells = ImGui::TreeNodeEx(fmt::format("{}-{}({})", cell_offset, cell_end, cell_size).c_str(),
                                           ImGuiTreeNodeFlags_SpanFullWidth);
        } else {
            ImGui::Text("%uz-%uz(%uz)", cell_offset, cell_end, cell_size);
        }

        ImGui::TableNextColumn();
        // TODO: somehow get cell coordinates.
        // tree view for multiple
        // auto grid_cell = m_solver->grid_cell(particle.position);
        // ImGui::Text("(%u, %u)", grid_cell.x, grid_cell.y);
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        if (list_cells) {
            for (const auto& [cell, cell_particle_indices] : grid_cell_to_particle_indices) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TableNextColumn();
                if (auto cell_hash = sph_solver::grid_hash(cell); ImGui::Selectable(
                        fmt::format("({}, {})", cell.x, cell.y).c_str(),
                                      cell_hash == m_selected_cell_index,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                    m_selected_cell_index = cell_hash;
                }
                ImGui::TableNextColumn();
                bool list_particles = false;
                if (!cell_particle_indices.empty()) {
                    list_particles =
                        ImGui::TreeNodeEx(fmt::format("Particles: {}", cell_particle_indices.size()).c_str(),
                                                   ImGuiTreeNodeFlags_SpanFullWidth);
                } else {
                    ImGui::Text("Particles: %uz", cell_particle_indices.size());
                }
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();

                if (list_particles) {
                    for (const auto& particle_index : cell_particle_indices) {
                        const auto& particle = m_solver->get_particles()[particle_index];
                        ImGui::TreeNodeEx("idk",
                                          ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet
                                              | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();
                        ImGui::Text("(%f, %f)", particle.position.x, particle.position.y);
                        ImGui::TableNextColumn();
                        ImGui::Text("(%f, %f)", particle.velocity.x, particle.velocity.y);
                        ImGui::TableNextColumn();
                        ImGui::Text("%f", particle.density);
                        ImGui::TableNextColumn();
                        ImGui::Text("%f", particle.property);

                        // TODO: particle index???
                    }

                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
    }

    glm::vec2 sph::screen_to_simulation(const glm::vec2& screen_pos) const
    {
        return render_area_to_simulation(screen_to_render_area(screen_pos));
    }

    glm::vec2 sph::screen_to_render_area(const glm::vec2& screen_pos) const
    {
        return screen_pos - m_render_offset;
    }

    glm::vec2 sph::simulation_to_screen(const glm::vec2& simulation_pos) const
    {
        return render_area_to_screen(simulation_to_render_area(simulation_pos));
    }

    glm::vec2 sph::render_area_to_screen(const glm::vec2& render_pos) const
    {
        return m_render_offset + render_pos;
    }

    glm::vec2 sph::simulation_to_render_area(const glm::vec2& simulation_pos) const
    {
        return glm::vec2{0.f, m_render_size.y} + glm::vec2{1.f, -1.f} * simulation_pos * (m_render_size / m_simulation_size);

    }

    glm::vec2 sph::render_area_to_simulation(const glm::vec2& render_pos) const
    {
        return glm::vec2{1.f, -1.f} * (render_pos - glm::vec2{0.f, m_render_size.y})
               * (m_simulation_size / m_render_size);
    }
}

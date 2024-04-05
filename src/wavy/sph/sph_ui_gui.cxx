/**
 * @file   sph_gui.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.03
 *
 * @brief  Module implementation for the GUI of the SPH solver.
 */

module;

#include "main.h"
#include "utils/enumerate.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <glm/common.hpp>
#include <glm/vec2.hpp>

module wavy.sphui:gui;
import :gui;

template<> struct std::hash<glm::uvec2>
{
    std::size_t operator()(const glm::uvec2& k) const { return wavy::sph::sph_solver::grid_hash(k); }
};

namespace wavy::sph {

    void sph_gui::draw_gui(sph_solver& solver)
    {
        if (ImGui::Begin("SPH")) {
            if (ImGui::BeginTabBar("SPH#tabs_bar")) {
                draw_settings_gui(solver);
                draw_particle_info_gui(solver);
                draw_cell_info_gui(solver);
                ImGui::EndTabBar();
            }

            ImGui::End();
        }
    }

    bool sph_gui::should_update_smoothing_kernels()
    {
        auto update = m_update_smoothing_kernels;
        m_update_smoothing_kernels = false;
        return update;
    }

    void sph_gui::select_particle(std::size_t selected_particle_index)
    {
        m_selected_particle_index = selected_particle_index;
    }

    void sph_gui::reset_selected_particle()
    {
        m_selected_particle_index = static_cast<std::size_t>(-1);
    }

    void sph_gui::reset_selected_cell()
    {
        m_selected_cell_hash = static_cast<std::size_t>(-1);
    }

    bool sph_gui::should_update_scalar_field()
    {
        auto update = m_update_scalar_field;
        m_update_scalar_field = false;
        return update;
    }

    void sph_gui::draw_settings_gui(sph_solver& solver)
    {
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::SliderFloat("Visual Particle Radius", &m_visual_particle_radius, .001f, 100.f);
            ImGui::SliderFloat("Simulation Time Scale", &m_sim_time_scale, .1f, 100000.f);
            ImGui::InputInt("Simulation Steps per Frame", &m_sim_steps_per_frame);
            m_sim_steps_per_frame = glm::max(1, m_sim_steps_per_frame);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            draw_simulation_settings_gui(solver);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            draw_particle_settings_gui(solver);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            draw_physical_settings_gui(solver);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            draw_scalar_visualization_settings_gui();

            ImGui::EndTabItem();
        }
    }

    void sph_gui::draw_simulation_settings_gui(sph_solver& solver)
    {
        if (auto particle_count = static_cast<int>(solver.get_particles().size());
            ImGui::InputInt("Particle Count", &particle_count)) {
            solver.set_particle_count(static_cast<std::size_t>(particle_count));
            m_update_scalar_field = true;
            m_selected_particle_index = static_cast<std::size_t>(-1);
        }

        std::array<const char*, 2> pattern_names{"random", "centered_grid"};
        if (auto current_pattern = static_cast<int>(solver.get_pattern());
            ImGui::Combo("Pattern", &current_pattern, pattern_names.data(), static_cast<int>(pattern_names.size()))) {
            solver.set_particle_pattern(static_cast<particle_pattern>(current_pattern));
            m_update_scalar_field = true;
            m_selected_particle_index = static_cast<std::size_t>(-1);
        }

        static int seed = 1337;
        if (solver.get_pattern() == particle_pattern::random) { ImGui::InputInt("Seed", &seed, 0); }

        if (ImGui::Button("Reset Particles")) {
            solver.reset_particles(seed);
            m_update_scalar_field = true;
            m_selected_particle_index = static_cast<std::size_t>(-1);
        }
    }

    void sph_gui::draw_particle_settings_gui(sph_solver& solver)
    {
        if (auto particle_radius = solver.get_config().get_particle_radius();
            ImGui::SliderFloat("Particle Radius", &particle_radius, .1f, 500.f)) {
            solver.get_config().set_particle_radius(particle_radius);
            m_update_smoothing_kernels = true;
            m_update_scalar_field = true;
        }

        if (auto particle_mass = solver.get_config().get_particle_mass();
            ImGui::SliderFloat("Particle Mass", &particle_mass, .001f, 100.f)) {
            solver.get_config().set_particle_mass(particle_mass);
            m_update_scalar_field = true;
        }
    }

    void sph_gui::draw_physical_settings_gui(sph_solver& solver)
    {
        if (auto gravity = solver.get_config().get_gravity(); ImGui::SliderFloat("Gravity", &gravity, .0f, 100.f)) {
            solver.get_config().set_gravity(gravity);
        }

        if (auto collision_dampening = solver.get_config().get_collision_dampening();
            ImGui::SliderFloat("Collision Dampening", &collision_dampening, .0f, 1.f)) {
            solver.get_config().set_collision_dampening(collision_dampening);
        }

        if (auto target_density = solver.get_config().get_target_density();
            ImGui::SliderFloat("Target Density", &target_density, .001f, 10.f)) {
            solver.get_config().set_target_density(target_density);
        }

        if (auto pressure_multiplier = solver.get_config().get_pressure_multiplier();
            ImGui::SliderFloat("Pressure Multiplier", &pressure_multiplier, .1f, 10.f)) {
            solver.get_config().set_pressure_multiplier(pressure_multiplier);
        }
    }

    void sph_gui::draw_scalar_visualization_settings_gui()
    {
        if (bool visualize_scalar = m_visualize_scalar != -1;
            ImGui::Checkbox("Visualize Density Field", &visualize_scalar)) {
            m_visualize_scalar = visualize_scalar ? 0 : -1;
        }

        if (m_visualize_scalar != -1) {
            ImGui::Checkbox("As Field", &m_show_scalar_field_texture);
            m_update_scalar_field |= ImGui::RadioButton("Density", &m_visualize_scalar, 0);
            m_update_scalar_field |= ImGui::RadioButton("Dummy Property", &m_visualize_scalar, 1);
        }
    }

    void sph_gui::draw_particle_info_gui(sph_solver& solver)
    {
        if (ImGui::BeginTabItem("Particle Info")) {
            if (ImGui::BeginTable("particle_props", 6,
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY
                                      | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings)) {
                ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 160);
                ImGui::TableSetupColumn("Velocity", ImGuiTableColumnFlags_WidthFixed, 120);
                ImGui::TableSetupColumn("Density", ImGuiTableColumnFlags_WidthFixed, 90);
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 90);
                ImGui::TableSetupColumn("Grid Cell", ImGuiTableColumnFlags_WidthFixed, 85);
                ImGui::TableSetupColumn("Grid Index", ImGuiTableColumnFlags_WidthFixed, 70);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                for (const auto& [index, particle] : utils::enumerate(solver.get_particles())) {
                    draw_particle_info_table_rows(solver, index, particle);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
    }

    void sph_gui::draw_particle_info_table_rows(sph_solver& solver, std::size_t index,
                                                const sph_solver::particle& particle)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Selectable(fmt::format("({:.2f}, {:.2f})", particle.position.x, particle.position.y).c_str(),
                              index == m_selected_particle_index,
                              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
            m_selected_particle_index = index;
        }
        ImGui::TableNextColumn();
        ImGui::Text("(%.2f, %.2f)", particle.velocity.x, particle.velocity.y);
        ImGui::TableNextColumn();
        ImGui::Text("%.4f", particle.density);
        ImGui::TableNextColumn();
        ImGui::Text("%.4f", particle.property);
        ImGui::TableNextColumn();
        auto grid_cell = solver.grid_cell(particle.position);
        ImGui::Text("[%u, %u]", grid_cell.x, grid_cell.y);
        ImGui::TableNextColumn();
        ImGui::Text("%zu", particle.grid_index);
    }

    void sph_gui::draw_cell_info_gui(sph_solver& solver)
    {
        if (ImGui::BeginTabItem("Cell Info")) {
            if (ImGui::BeginTable("cell_props", 8,
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY
                                      | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings)) {
                ImGui::TableSetupColumn("Grid Index", ImGuiTableColumnFlags_WidthFixed, 70);
                ImGui::TableSetupColumn("Particle Count", ImGuiTableColumnFlags_WidthFixed, 100);
                ImGui::TableSetupColumn("Grid Cells", ImGuiTableColumnFlags_WidthFixed, 70);
                ImGui::TableSetupColumn("Particle", ImGuiTableColumnFlags_WidthFixed, 65);
                ImGui::TableSetupColumn("-Position", ImGuiTableColumnFlags_WidthFixed, 135);
                ImGui::TableSetupColumn("-Velocity", ImGuiTableColumnFlags_WidthFixed, 135);
                ImGui::TableSetupColumn("-Density", ImGuiTableColumnFlags_WidthFixed, 65);
                ImGui::TableSetupColumn("-Property", ImGuiTableColumnFlags_WidthFixed, 65);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                for (const auto& [index, cell] : utils::enumerate(solver.get_cell_sizes())) {
                    draw_cell_info_table_rows(solver, index, static_cast<std::size_t>(cell.load()));
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
    }

    void sph_gui::draw_cell_info_table_rows(sph_solver& solver, std::size_t index, std::size_t cell_size)
    {
        auto cell_offset = solver.get_cell_offsets()[index];
        auto cell_end = cell_offset + cell_size;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%zu", index);
        ImGui::TableNextColumn();
        bool list_cells = false;
        if (cell_size != 0) {
            list_cells =
                ImGui::TreeNodeEx(fmt::format("{}: {}-{}###cnt{}", cell_size, cell_offset, cell_end, index).c_str(),
                                  ImGuiTreeNodeFlags_SpanFullWidth);
        } else {
            ImGui::Text("%zu: %zu-%zu", cell_size, cell_offset, cell_end, index);
        }

        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        if (list_cells) {
            draw_cell_info_table_list_cells(solver, cell_offset, cell_end);
            ImGui::TreePop();
        }
    }

    void sph_gui::draw_cell_info_table_list_cells(sph_solver& solver, std::size_t cell_offset, std::size_t cell_end)
    {
        std::unordered_map<glm::uvec2, std::vector<std::size_t>> grid_cell_to_particle_indices;
        for (std::size_t particle_index_i = cell_offset; particle_index_i < cell_end; ++particle_index_i) {
            auto particle_index = solver.get_particle_indices()[particle_index_i];
            const auto& particle = solver.get_particles()[particle_index];
            grid_cell_to_particle_indices[solver.grid_cell(particle.position)].push_back(particle_index);
        }
        for (const auto& [cell, cell_particle_indices] : grid_cell_to_particle_indices) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            if (auto cell_hash = sph_solver::grid_hash(cell);
                ImGui::Selectable(fmt::format("[{}, {}]", cell.x, cell.y).c_str(), cell_hash == m_selected_cell_hash,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                m_selected_cell_hash = cell_hash;
            }
            ImGui::TableNextColumn();
            bool list_particles = false;
            if (!cell_particle_indices.empty()) {
                list_particles = ImGui::TreeNodeEx(fmt::format("#: {}", cell_particle_indices.size()).c_str(),
                                                   ImGuiTreeNodeFlags_SpanFullWidth);
            } else {
                ImGui::Text("#: %zu", cell_particle_indices.size());
            }
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();

            if (list_particles) {
                draw_cell_info_table_list_particles(solver, cell_particle_indices);
                ImGui::TreePop();
            }
        }
    }

    void sph_gui::draw_cell_info_table_list_particles(sph_solver& solver,
                                                      const std::vector<std::size_t>& cell_particle_indices)
    {
        for (const auto& particle_index : cell_particle_indices) {
            const auto& particle = solver.get_particles()[particle_index];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::TreeNodeEx(fmt::format("{}", particle_index).c_str(),
                              ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen
                                  | ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::SameLine();
            if (ImGui::Selectable(fmt::format("##{}", particle_index).c_str(),
                                  particle_index == m_selected_particle_index,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                m_selected_particle_index = particle_index;
            }
            ImGui::TableNextColumn();
            ImGui::Text("(%.2f, %.2f)", particle.position.x, particle.position.y);
            ImGui::TableNextColumn();
            ImGui::Text("(%.2f, %.2f)", particle.velocity.x, particle.velocity.y);
            ImGui::TableNextColumn();
            ImGui::Text("%.4f", particle.density);
            ImGui::TableNextColumn();
            ImGui::Text("%.4f", particle.property);
        }
    }
}

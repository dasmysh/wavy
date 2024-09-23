/**
 * @file   sph_gui.cppm
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.03
 *
 * @brief  Module for the GUI of the SPH solver.
 */

module;

#include "sph/sph_solver.h"
#include <vector>

export module wavy.sphui:gui;

namespace wavy::sph {
    export class sph_gui
    {
    public:
        void draw_gui(sph_solver& solver);

        bool should_update_smoothing_kernels();

        float get_visual_particle_radius() const { return m_visual_particle_radius; }
        float get_sim_time_scale() const { return m_sim_time_scale; }
        int get_sim_steps_per_frame() const { return m_sim_steps_per_frame; }

        std::size_t get_selected_particle_index() const { return m_selected_particle_index; }
        void select_particle(std::size_t selected_particle_index);
        void reset_selected_particle();

        std::size_t get_selected_cell_hash() const { return m_selected_cell_hash; }
        void reset_selected_cell();

        bool should_update_scalar_field();
        int get_visualize_scalar() const { return m_visualize_scalar; }
        bool is_show_scalar_field_texture() const { return m_show_scalar_field_texture; }

    private:
        void draw_settings_gui(sph_solver& solver);
        void draw_simulation_settings_gui(sph_solver& solver);
        void draw_particle_settings_gui(sph_solver& solver);
        void draw_physical_settings_gui(sph_solver& solver);
        void draw_scalar_visualization_settings_gui();

        void draw_particle_info_gui(sph_solver& solver);
        void draw_particle_info_table_rows(sph_solver& solver, std::size_t index, const sph_solver::particle& particle);
        void draw_cell_info_gui(sph_solver& solver);
        void draw_cell_info_table_rows(sph_solver& solver, std::size_t index, std::size_t cell_size);
        void draw_cell_info_table_list_cells(sph_solver& solver, std::size_t cell_offset, std::size_t cell_end);
        void draw_cell_info_table_list_particles(sph_solver& solver,
                                                 const std::vector<std::size_t>& cell_particle_indices);

        float m_visual_particle_radius = 5.f;
        float m_sim_time_scale = 0.1f;
        int m_sim_steps_per_frame = 2;

        std::size_t m_selected_particle_index = static_cast<std::size_t>(-1);
        std::size_t m_selected_cell_hash = static_cast<std::size_t>(-1);

        bool m_update_scalar_field = true;
        int m_visualize_scalar = -1;
        bool m_show_scalar_field_texture = false;

        bool m_update_smoothing_kernels = false;
    };

}

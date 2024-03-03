/**
 * @file   sph.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief Managing class for a SPH(smooth particle hydrodynamics) solver.
 */

#pragma once

#include "sph_solver.h"

#include <SFML/Graphics.hpp>
#include <glm/vec2.hpp>
#include <array>
#include <memory>

namespace wavy::sph {

    class sph
    {
    public:
        explicit sph(const glm::vec2& sim_area);
        ~sph();

        void simulation_frame(float delta_t);

        void draw_gui();
        void draw_simulation(sf::RenderTarget& rt);
        void process_event(const sf::Event& event);

    private:
        void visualize_scalar_field_points(sf::RenderTarget& rt, std::size_t i) const;
        void visualize_scalar_field(sf::RenderTarget& rt, std::size_t i) const;
        void draw_grid(sf::RenderTarget& rt) const;


        void update_smoothing_kernels();
        void update_smoothing_kernel(std::size_t i, unsigned int radius);
        void update_scalar_field_texture(std::size_t i) const;
        sf::Color calculate_scalar_color_at(const glm::vec2& sim_position, std::size_t i, float scale) const;

        void draw_settings_gui();
        void draw_particle_info_gui();
        void draw_particle_info_table_rows(std::size_t index, const sph_solver::particle& particle);
        void draw_cell_info_gui();
        void draw_cell_info_table_rows(std::size_t index, std::size_t cell_size);
        void draw_cell_info_table_list_cells(std::size_t cell_offset, std::size_t cell_end);
        void draw_cell_info_table_list_particles(const std::vector<std::size_t>& cell_particle_indices);

        glm::vec2 screen_to_simulation(const glm::vec2& screen_pos) const;
        glm::vec2 screen_to_render_area(const glm::vec2& screen_pos) const;
        glm::vec2 simulation_to_screen(const glm::vec2& simulation_pos) const;
        glm::vec2 render_area_to_screen(const glm::vec2& render_pos) const;
        glm::vec2 simulation_to_render_area(const glm::vec2& simulation_pos) const;
        glm::vec2 render_area_to_simulation(const glm::vec2& render_pos) const;

        glm::vec2 m_simulation_size;
        std::unique_ptr<sph_solver> m_solver;
        float m_visual_particle_radius = 5.f;
        float m_sim_time_scale = 10.f;
        int m_sim_steps_per_frame = 10;

        float m_last_delta_t = 0.016f;
        bool m_delta_t_out_of_bounds = false;

        sf::Font m_delta_t_font;
        std::array<sf::Texture, 2> m_smoothing_kernels;

        int m_visualize_scalar = -1;
        mutable sf::Texture m_scalar_field_texture;
        bool m_show_scalar_field_texture = false;
        mutable bool m_update_scalar_field = true;
        mutable std::vector<unsigned int> m_screen_ys;

        mutable glm::vec2 m_screen_size;
        mutable glm::vec2 m_render_size;
        mutable glm::vec2 m_render_offset;

        std::size_t m_selected_particle_index = static_cast<std::size_t>(-1);
        std::size_t m_selected_cell_hash = static_cast<std::size_t>(-1);

        bool m_mouse_clicked = false;
        glm::vec2 m_mouse_pos_simulation{-1.f};
    };
}

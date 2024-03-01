/**
 * @file   sph.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief Managing class for a SPH(smooth particle hydrodynamics) solver.
 */

#pragma once

#include <SFML/Graphics.hpp>
#include <glm/vec2.hpp>
#include <array>
#include <memory>

namespace wavy::sph {

    class sph_solver;

    class sph
    {
    public:
        explicit sph(const glm::vec2& sim_area);
        ~sph();

        void simulation_frame(float delta_t);

        void draw_gui();
        void draw_simulation(sf::RenderTarget& rt) const;

    private:
        void visualize_scalar_field_points(sf::RenderTarget& rt, std::size_t i, const glm::vec2& area_offset,
                                           const glm::vec2& area_size) const;

        void visualize_scalar_field(sf::RenderTarget& rt, std::size_t i, const glm::vec2& area_offset,
                                    const glm::vec2& area_size) const;

        void update_smoothing_kernels();
        void update_smoothing_kernel(std::size_t i, unsigned int radius);
        void update_scalar_field_texture(const sf::RenderTarget& rt, std::size_t i, const glm::vec2& area_offset,
                                         const glm::vec2& area_size) const;
        sf::Color calculate_scalar_color_at(const glm::vec2& sim_position, std::size_t i, float scale) const;

        glm::vec2 m_sim_area;
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
    };
}

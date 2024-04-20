/**
 * @file   sph_ui_visualization.cppm
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.07
 *
 * @brief  Module for the visualization of the SPH solver.
 */

module;

#include <SFML/Graphics.hpp>
#include <glm/vec2.hpp>
#include <array>

export module wavy.sphui:visualization;
import :gui;
import :conversions;
import :input;
import :timer;

namespace wavy::sph
{
    export class sph_visualization
    {
    public:
        explicit sph_visualization(sph_solver& solver);
        void draw_simulation(sph_solver& solver, sph_conversions& conversions, sph_gui& gui, sph_input& input,
                             sph_timer& timer, sf::RenderTarget& rt);

        void update_smoothing_kernels(sph_solver& solver);

    private:
        void visualize_scalar_field_points(sph_solver& solver, sph_conversions& conversions, sf::RenderTarget& rt,
                                           std::size_t i) const;
        void visualize_scalar_field(sf::RenderTarget& rt) const;
        void draw_grid(sph_solver& solver, sph_conversions& conversions, sph_gui& gui, sf::RenderTarget& rt) const;

        void update_scalar_field_texture(sph_solver& solver, sph_conversions& conversions, std::size_t i) const;
        void update_smoothing_kernel(sph_solver& solver, std::size_t i, unsigned int radius);

        sf::Color calculate_scalar_color_at(sph_solver& solver, sph_conversions& conversions,
                                            const glm::vec2& sim_position, std::size_t i, float scale) const;


        mutable sf::Texture m_scalar_field_texture;
        mutable std::vector<unsigned int> m_screen_ys;

        sf::Font m_delta_t_font;
        std::array<sf::Texture, 2> m_smoothing_kernels;
    };
}

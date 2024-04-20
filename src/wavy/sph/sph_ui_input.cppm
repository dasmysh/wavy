/**
 * @file   sph_ui_input.cppm
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.19
 *
 * @brief  Module for SPH user input handling.
 */

module;

#include <SFML/Graphics.hpp>
#include <glm/vec2.hpp>

export module wavy.sphui:input;
import :gui;
import :conversions;

namespace wavy::sph {
    export class sph_input
    {
    public:
        sph_input() = default;

        void process_event(sph_conversions& conversions, sph_gui& gui, const sf::Event& event);

        bool is_mouse_clicked() const { return m_mouse_clicked; }
        const glm::vec2& get_mouse_pos_simulation() const { return m_mouse_pos_simulation; }

    private:
        bool m_mouse_clicked = false;
        glm::vec2 m_mouse_pos_simulation{-1.f};
    };
}

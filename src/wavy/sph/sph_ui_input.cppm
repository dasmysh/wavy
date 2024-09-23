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
#include <chrono>

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
        const glm::vec2& get_mouse_pos_screen() const { return m_mouse_pos_screen; }

        bool is_space_pressed() const { return m_space_pressed; }
        bool is_space_double_pressed() const { return m_space_double_pressed; }

        void reset_state();

    private:
        struct double_press_timer
        {
            std::chrono::system_clock::time_point time;
            bool timer_started;
            void start();
            bool check();
        };

        bool m_mouse_clicked = false;
        glm::vec2 m_mouse_pos_simulation{-1.f};
        glm::vec2 m_mouse_pos_screen{-1.f};

        bool m_space_pressed = false;
        bool m_space_double_pressed = false;
        double_press_timer m_space_double_timer;
    };
}

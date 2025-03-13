/**
 * @file   sph_ui_input.cxx
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.19
 *
 * @brief  Module implementation for SPH user input handling.
 */

module;

#include <glm/vec2.hpp>
#include <imgui.h>
#include <chrono>

module wavy.sphui:input;
import :input;

namespace wavy::sph {

    void sph_input::process_event(sph_conversions& conversions, sph_gui& gui, const sf::Event& event)
    {

        m_space_pressed = false;
        m_space_double_pressed = false;
        if (const auto& io = ImGui::GetIO(); !io.WantCaptureKeyboard) {
            if (event.type == sf::Event::KeyReleased && event.key.scancode == sf::Keyboard::Scancode::Space) {
                m_space_pressed = true;
                if (m_space_double_timer.check()) {
                    m_space_double_pressed = true;
                } else {
                    m_space_double_timer.start();
                }
            }

            if (event.key.scancode == sf::Keyboard::Scancode::P) {
                if (event.type == sf::Event::KeyPressed) {
                    m_p_down = true;
                } else if (event.type == sf::Event::KeyReleased) {
                    m_p_down = false;
                }
            }
        }

        m_mouse_clicked = false;
        m_mouse_wheel_delta = 0.f;
        if (const auto& io = ImGui::GetIO(); !io.WantCaptureMouse) {
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                set_mouse_position(conversions, event.mouseButton.x, event.mouseButton.y);
                if (!m_p_down) {
                    m_mouse_clicked = true;
                    gui.reset_selected_particle();
                }
                m_mouse_left_down = false;
            }

            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right) {
                if (!m_p_down) {
                    gui.reset_selected_particle();
                    gui.reset_selected_cell();
                }
                m_mouse_right_down = false;
            }

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left
                && m_p_down) {
                m_mouse_left_down = true;
            }

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right
                && m_p_down) {
                m_mouse_right_down = true;
            }

            if (event.type == sf::Event::MouseWheelScrolled
                && event.mouseWheelScroll.wheel == sf::Mouse::Wheel::VerticalWheel) {
                set_mouse_position(conversions, event.mouseWheelScroll.x, event.mouseWheelScroll.y);
                m_mouse_wheel_delta = static_cast<float>(event.mouseWheelScroll.delta);
            }

            if (event.type == sf::Event::MouseMoved) {
                set_mouse_position(conversions, event.mouseMove.x, event.mouseMove.y);
            }
        }
    }

    void sph_input::reset_state()
    {
        m_mouse_clicked = false;
        m_space_pressed = false;
        m_space_double_pressed = false;
    }

    void sph_input::set_mouse_position(sph_conversions& conversions, int x, int y)
    {
        m_mouse_pos_screen =
            glm::vec2{static_cast<float>(x), static_cast<float>(y)};
        m_mouse_pos_simulation = conversions.screen_to_simulation(m_mouse_pos_screen);
    }

    void sph_input::double_press_timer::start()
    {
        assert(!timer_started && "Timer not in a valid state.");
        timer_started = true;
        time = std::chrono::system_clock::now();
    }

    bool sph_input::double_press_timer::check()
    {
        using namespace std::chrono_literals;
        constexpr auto time_delta = 300ms;
        bool result = false;
        const auto current_time = std::chrono::system_clock::now();
        if (timer_started && current_time - time < time_delta) { result = true; }

        timer_started = false;
        return result;
    }
}

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
            if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>(); keyPressed) {
                if (keyPressed->scancode == sf::Keyboard::Scancode::Space) {
                    m_space_pressed = true;
                    if (m_space_double_timer.check()) {
                        m_space_double_pressed = true;
                    } else {
                        m_space_double_timer.start();
                    }
                } else if (keyPressed->scancode == sf::Keyboard::Scancode::P) {
                    m_p_down = true;
                }
            } else if (const auto* keyReleased = event.getIf<sf::Event::KeyReleased>(); keyReleased) {
                if (keyReleased->scancode == sf::Keyboard::Scancode::P) {
                    m_p_down = false;
                }
            }
        }

        m_mouse_clicked = false;
        m_mouse_wheel_delta = 0.f;
        if (const auto& io = ImGui::GetIO(); !io.WantCaptureMouse) {
            if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>(); mouseReleased) {
                if (mouseReleased->button == sf::Mouse::Button::Left) {
                    set_mouse_position(conversions, mouseReleased->position.x, mouseReleased->position.y);
                    if (!m_p_down) {
                        m_mouse_clicked = true;
                        gui.reset_selected_particle();
                    }
                    m_mouse_left_down = false;
                } else if (mouseReleased->button == sf::Mouse::Button::Right) {
                    if (!m_p_down) {
                        gui.reset_selected_particle();
                        gui.reset_selected_cell();
                    }
                    m_mouse_right_down = false;
                }
            } else if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>(); mousePressed) {
                if (mousePressed->button == sf::Mouse::Button::Left && m_p_down) {
                    m_mouse_left_down = true;
                } else if (mousePressed->button == sf::Mouse::Button::Right && m_p_down) {
                    m_mouse_right_down = true;
                }
            } else if (const auto* mouseScrolled = event.getIf<sf::Event::MouseWheelScrolled>(); mouseScrolled) {
                if (mouseScrolled->wheel == sf::Mouse::Wheel::Vertical) {
                    set_mouse_position(conversions, mouseScrolled->position.x, mouseScrolled->position.y);
                    m_mouse_wheel_delta = static_cast<float>(mouseScrolled->delta);
                }
            } else if (const auto* mouseMoved = event.getIf<sf::Event::MouseMoved>(); mouseMoved) {
                set_mouse_position(conversions, mouseMoved->position.x, mouseMoved->position.y);
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

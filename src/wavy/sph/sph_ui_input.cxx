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
        m_mouse_clicked = false;
        if (const auto& io = ImGui::GetIO(); !io.WantCaptureMouse) {
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                m_mouse_pos_screen = glm::vec2{static_cast<float>(event.mouseButton.x),
                                           static_cast<float>(event.mouseButton.y)};
                m_mouse_pos_simulation = conversions.screen_to_simulation(m_mouse_pos_screen);
                m_mouse_clicked = true;
                gui.reset_selected_particle();
            }

            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right) {
                gui.reset_selected_particle();
                gui.reset_selected_cell();
            }
        }

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
        }
    }

    void sph_input::reset_state()
    {
        m_mouse_clicked = false;
        m_space_pressed = false;
        m_space_double_pressed = false;
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

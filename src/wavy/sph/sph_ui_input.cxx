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

module wavy.sphui:input;
import :input;

namespace wavy::sph {

    void sph_input::process_event(sph_conversions& conversions, sph_gui& gui, const sf::Event& event)
    {
        m_mouse_clicked = false;
        if (const auto& io = ImGui::GetIO(); !io.WantCaptureMouse) {
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                glm::vec2 mouse_pos_screen{static_cast<float>(event.mouseButton.x),
                                           static_cast<float>(event.mouseButton.y)};
                m_mouse_pos_simulation = conversions.screen_to_simulation(mouse_pos_screen);
                m_mouse_clicked = true;
                gui.reset_selected_particle();
            }

            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right) {
                gui.reset_selected_particle();
                gui.reset_selected_cell();
            }
        }
    }
}

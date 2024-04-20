/**
 * @file   sph_ui_conversions.cppm
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.07
 *
 * @brief  Module for SPH coordinate system conversions.
 */

module;

#include <glm/vec2.hpp>

export module wavy.sphui:conversions;

namespace wavy::sph
{
    export class sph_conversions
    {
    public:
        explicit sph_conversions(const glm::vec2& sim_area);

        void update_metrics(const glm::vec2 screen_size);

        // void set_simulation_size(const glm::vec2 simulation_size);
        glm::vec2 get_simulation_size() const { return m_simulation_size; }
        glm::vec2 get_screen_size() const { return m_screen_size; }
        glm::vec2 get_render_size() const { return m_render_size; }

        glm::vec2 screen_to_simulation(const glm::vec2& screen_pos) const;
        glm::vec2 screen_to_render_area(const glm::vec2& screen_pos) const;
        glm::vec2 simulation_to_screen(const glm::vec2& simulation_pos) const;
        glm::vec2 render_area_to_screen(const glm::vec2& render_pos) const;
        glm::vec2 simulation_to_render_area(const glm::vec2& simulation_pos) const;
        glm::vec2 render_area_to_simulation(const glm::vec2& render_pos) const;

    private:
        glm::vec2 m_simulation_size;
        glm::vec2 m_screen_size;
        glm::vec2 m_render_size;
        glm::vec2 m_render_offset;
    };


}

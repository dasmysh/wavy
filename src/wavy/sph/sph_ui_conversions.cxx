/**
 * @file   sph_ui_conversions.cxx
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.07
 *
 * @brief  Module implementation for SPH conversions.
 */

module;

#include <glm/vec2.hpp>

module wavy.sphui:conversions;
import :conversions;

namespace wavy::sph
{
    sph_conversions::sph_conversions(const glm::vec2& sim_area)
        : m_simulation_size{sim_area}
    {}

    void sph_conversions::update_metrics(const glm::vec2 screen_size)
    {
        constexpr float border_size_ratio = .05f;
        constexpr float render_area_size_ratio = 1.f - 2.f * border_size_ratio;
        m_screen_size = screen_size;
        m_render_offset = glm::vec2{border_size_ratio * m_screen_size.x, border_size_ratio * m_screen_size.y};
        m_render_size = glm::vec2{render_area_size_ratio * m_screen_size.x, render_area_size_ratio * m_screen_size.y};
    }

    glm::vec2 sph_conversions::screen_to_simulation(const glm::vec2& screen_pos) const
    {
        return render_area_to_simulation(screen_to_render_area(screen_pos));
    }

    glm::vec2 sph_conversions::screen_to_render_area(const glm::vec2& screen_pos) const
    {
        return screen_pos - m_render_offset;
    }

    glm::vec2 sph_conversions::simulation_to_screen(const glm::vec2& simulation_pos) const
    {
        return render_area_to_screen(simulation_to_render_area(simulation_pos));
    }

    glm::vec2 sph_conversions::render_area_to_screen(const glm::vec2& render_pos) const
    {
        return m_render_offset + render_pos;
    }

    glm::vec2 sph_conversions::simulation_to_render_area(const glm::vec2& simulation_pos) const
    {
        return glm::vec2{0.f, m_render_size.y}
               + glm::vec2{1.f, -1.f} * simulation_pos * (m_render_size / m_simulation_size);
    }

    glm::vec2 sph_conversions::render_area_to_simulation(const glm::vec2& render_pos) const
    {
        return glm::vec2{1.f, -1.f} * (render_pos - glm::vec2{0.f, m_render_size.y})
               * (m_simulation_size / m_render_size);
    }
}

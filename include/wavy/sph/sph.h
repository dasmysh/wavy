/**
 * @file   sph.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief Managing class for a SPH(smooth particle hydrodynamics) solver.
 */

#pragma once

#include <glm/vec2.hpp>
#include <memory>

namespace sf {
    class RenderTarget;
}

namespace wavy::sph {

    class sph_solver;

    class sph
    {
    public:
        explicit sph(const glm::vec2& sim_area);
        ~sph();

        void draw(sf::RenderTarget& rt);

        void draw_gui();
        void draw_simulation(sf::RenderTarget& rt) const;

    private:
        glm::vec2 m_sim_area;
        std::unique_ptr<sph_solver> m_solver;
        float m_visual_particle_radius = 5.f;
        float m_sim_time_scale = 300.f;
        int m_sim_steps_per_frame = 10;
    };
}

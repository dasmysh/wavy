/**
 * @file   sph.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief Managing class for a SPH(smooth particle hydrodynamics) solver.
 */

#pragma once

import wavy.sphui;

#include "sph_solver.h"

#include <SFML/Graphics.hpp>
#include <glm/vec2.hpp>
#include <array>
#include <memory>

namespace wavy::sph {

    class sph
    {
    public:
        explicit sph(const glm::vec2& sim_area);
        ~sph();

        void simulation_frame(float delta_t);

        void draw_gui();
        void draw_simulation(sf::RenderTarget& rt);
        void process_event(const sf::Event& event);

    private:
        // void update_smoothing_kernels();

        std::unique_ptr<sph_solver> m_solver;
        sph_conversions m_conversions;
        sph_gui m_gui;
        sph_visualization m_visualization;
        sph_input m_input;
        sph_timer m_timer;
    };
}

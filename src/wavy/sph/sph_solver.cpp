/**
 * @file   sph_solver.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief  Implementation for the SPH solver.
 */

#include "sph/sph_solver.h"
#include "utils/enumerate.h"
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <algorithm>
#include <random>

namespace wavy::sph {

    sph_solver::sph_solver(const glm::vec2& simulation_area, std::size_t initial_particle_count /*= 100*/,
                           particle_pattern pattern /*= particle_pattern::random */)
        : m_simulation_area{simulation_area}
        , m_particles{initial_particle_count}
        , m_pattern{pattern}
    {
        reset_particles();
    }

    sph_solver::~sph_solver() = default;

    void sph_solver::set_particle_count(std::size_t particle_count)
    {
        m_particles.resize(particle_count);
        reset_particles();
    }

    void sph_solver::set_particle_pattern(particle_pattern pattern)
    {
        m_pattern = pattern;
        reset_particles();
    }

    void sph_solver::reset_particles(unsigned int seed /*= 1337*/)
    {
        if (m_pattern == particle_pattern::random) {
            std::mt19937 eng{ seed };
            std::uniform_real_distribution<float> distr_x(0.f, m_simulation_area.x);
            std::uniform_real_distribution<float> distr_y(0.f, m_simulation_area.y);

            std::ranges::for_each(m_particles, [&distr_x, &distr_y, &eng](auto& particle) {
                particle.position = glm::vec2{distr_x(eng), distr_y(eng)};
            });
        } else if (m_pattern == particle_pattern::centered_grid) {
            auto grid_width = glm::ceil(glm::sqrt(static_cast<float>(m_particles.size())));
            auto pos_offset = .5f * m_simulation_area
                              + glm::vec2{-.5f, .5f} * grid_width * m_particle_radius;

            std::ranges::for_each(utils::enumerate(m_particles), [this, grid_width, &pos_offset](auto enum_particle) {
                auto i = static_cast<float>(std::get<0>(enum_particle));
                auto x = glm::mod(i, grid_width);
                auto y = -glm::floor(i / grid_width) -.5f;
                std::get<1>(enum_particle).position =
                    pos_offset + glm::vec2{x, y} * m_particle_radius;
                });
        }
    }
}

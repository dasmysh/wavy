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

    void sph_solver::simulation_step(float delta_t)
    {
        simulate_gravity(delta_t);
        resolve_collisions();
    }

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
                particle.velocity = glm::vec2{0.f};
            });
        } else if (m_pattern == particle_pattern::centered_grid) {
            auto grid_width = glm::ceil(glm::sqrt(static_cast<float>(m_particles.size())));
            auto pos_offset = .5f * (m_simulation_area - grid_width * m_particle_radius);

            std::ranges::for_each(utils::enumerate(m_particles), [this, grid_width, &pos_offset](auto enum_particle) {
                auto i = static_cast<float>(std::get<0>(enum_particle));
                auto x = glm::mod(i, grid_width);
                auto y = glm::floor((static_cast<float>(m_particles.size() - 1) - i) / grid_width) + .5f;

                auto& particle = std::get<1>(enum_particle);
                particle.position = pos_offset + glm::vec2{x, y} * m_particle_radius;
                particle.velocity = glm::vec2{0.f};
                });
        }
    }

    void sph_solver::simulate_gravity(float delta_t)
    {
        float delta_gravity = m_gravity * delta_t;
        std::ranges::for_each(m_particles, [delta_gravity, delta_t](auto& particle) {
            particle.velocity += glm::vec2{0.f, -1.f} * delta_gravity;
            particle.position += particle.velocity * delta_t;
        });
    }

    void sph_solver::resolve_collisions()
    {
        std::ranges::for_each(m_particles, [this](auto& particle) {
            if (particle.position.x < m_particle_radius) {
                particle.position.x = m_particle_radius;
                particle.velocity.x *= -1.f * (1.f - m_collision_dampening);
            }
            if (particle.position.x > m_simulation_area.x - m_particle_radius) {
                particle.position.x = m_simulation_area.x - m_particle_radius;
                particle.velocity.x *= -1.f * (1.f - m_collision_dampening);
            }
            if (particle.position.y < m_particle_radius) {
                particle.position.y = m_particle_radius;
                particle.velocity.y *= -1.f * (1.f - m_collision_dampening);
            }
            if (particle.position.y > m_simulation_area.y - m_particle_radius) {
                particle.position.y = m_simulation_area.y - m_particle_radius;
                particle.velocity.y *= -1.f * (1.f - m_collision_dampening);
            }
        });
    }
}

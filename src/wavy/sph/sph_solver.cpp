/**
 * @file   sph_solver.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief  Implementation for the SPH solver.
 */

#include "main.h"
#include "sph/sph_solver.h"
#include "utils/enumerate.h"
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <random>
#include <execution>

namespace wavy::sph {

    sph_solver::sph_solver(const glm::vec2& simulation_area, std::size_t initial_particle_count /*= 100*/,
                           particle_pattern pattern /*= particle_pattern::random */)
        : m_simulation_area{simulation_area}
        , m_particles(initial_particle_count)
        , m_particle_indices(initial_particle_count)
        , m_particle_histogram(initial_particle_count)
        , m_pattern{pattern}
    {
        reset_particles();
    }

    sph_solver::~sph_solver() = default;

    void sph_solver::simulation_step(float delta_t)
    {
        std::for_each(std::execution::par, std::begin(m_particle_histogram), std::end(m_particle_histogram),
                      [](auto& v) { v = 0; });
        simulate_gravity(delta_t);
        resolve_collisions();

        // TODO

        update_densities();
    }

    void sph_solver::set_particle_count(std::size_t particle_count)
    {
        m_particles.resize(particle_count);
        m_particle_indices.resize(particle_count);
        m_particle_histogram = std::vector<std::atomic_int>(particle_count);
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
            std::uniform_real_distribution<float> distr_x(m_particle_radius, m_simulation_area.x - m_particle_radius);
            std::uniform_real_distribution<float> distr_y(m_particle_radius, m_simulation_area.y - m_particle_radius);

            std::ranges::for_each(m_particles, [this, &distr_x, &distr_y, &eng](auto& prtcl) {
                prtcl = particle{glm::vec2{distr_x(eng), distr_y(eng)}};
                prtcl.property = calc_property(prtcl.position);
            });
        } else if (m_pattern == particle_pattern::centered_grid) {
            auto grid_width = glm::ceil(glm::sqrt(static_cast<float>(m_particles.size())));
            auto pos_offset = .5f * (m_simulation_area - grid_width * m_particle_radius);

            std::ranges::for_each(utils::enumerate(m_particles), [this, grid_width, &pos_offset](auto enum_particle) {
                auto i = static_cast<float>(std::get<0>(enum_particle));
                auto x = glm::mod(i, grid_width);
                auto y = glm::floor((static_cast<float>(m_particles.size() - 1) - i) / grid_width) + .5f;

                auto& prtcl = std::get<1>(enum_particle);
                prtcl = particle{pos_offset + glm::vec2{x, y} * m_particle_radius};
                prtcl.property = calc_property(prtcl.position);
                });
        }
    }

    void sph_solver::simulate_gravity(float delta_t)
    {
        float delta_gravity = m_gravity * delta_t;
        std::for_each(std::execution::par, std::begin(m_particles), std::end(m_particles),
                      [delta_gravity, delta_t](auto& particle) {
            particle.velocity += glm::vec2{0.f, -1.f} * delta_gravity;
            particle.position += particle.velocity * delta_t;
        });
    }

    void sph_solver::resolve_collisions()
    {
        std::for_each(std::execution::par, std::begin(m_particles), std::end(m_particles), [this](auto& particle) {
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

            particle.grid_index = grid_hash(grid_cell(particle.position)) % m_particle_indices.size();
            m_particle_histogram[particle.grid_index] += 1;
        });
    }

    void sph_solver::update_densities()
    {
        std::for_each(std::execution::par, std::begin(m_particles), std::end(m_particles),
                      [this](auto& particle) { particle.density = calculate_density(particle.position); });
    }

    glm::uvec2 sph_solver::grid_cell(const glm::vec2& p) const
    {
        return glm::uvec2{glm::floor(p / m_particle_radius)};
    }

    std::size_t sph_solver::grid_hash(const glm::uvec2& cell)
    {
        constexpr std::size_t prime_x = 94847;
        constexpr std::size_t prime_y = 31699;
        return cell.x * prime_x + cell.y * prime_y;
    }

    float sph_solver::calculate_density(const glm::vec2& p) const
    {
        float density = 0.f;

        // TODO: only iterate over nearby particles
        std::ranges::for_each(m_particles, [this, &density, &p](auto& particle) {
            float r = glm::length(particle.position - p);
            float influence = density_kernel(r);
            density += m_particle_mass * influence;
        });

        return density;
    }

    float sph_solver::calculate_property(const glm::vec2& p) const
    {
        float property = 0.f;

        // TODO: only iterate over nearby particles
        std::ranges::for_each(m_particles, [this, &property, &p](auto& particle) {
            float r = glm::length(particle.position - p);
            float influence = property_kernel(r);
            float density = particle.density;
            property += particle.property * m_particle_mass * influence / density;
        });

        return property;
    }

    float sph_solver::density_kernel(float r) const
    {
        float volume = glm::pi<float>() * glm::pow(m_particle_radius, 8.f) / 4.f;
        float value = glm::max(0.f, m_particle_radius * m_particle_radius - r * r);
        return value * value * value / volume;
    }

    float sph_solver::property_kernel(float r) const
    {
        // TODO: same as density for now.
        float volume = glm::pi<float>() * glm::pow(m_particle_radius, 8.f) / 4.f;
        float value = glm::max(0.f, m_particle_radius * m_particle_radius - r * r);
        return value * value * value / volume;
    }

    float sph_solver::calc_property(const glm::vec2& p)
    {
        return glm::cos(p.y - 3.f + glm::sin(p.x));
    }

}

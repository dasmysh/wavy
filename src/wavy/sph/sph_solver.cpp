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
#include <barrier>

namespace wavy::sph {

    sph_solver::sph_solver(const glm::vec2& simulation_area, std::size_t initial_particle_count /*= 100*/,
                           particle_pattern pattern /*= particle_pattern::random */)
        : m_simulation_area{simulation_area}
        , m_particles(initial_particle_count)
        , m_particle_indices(initial_particle_count)
        , m_particle_histogram(initial_particle_count)
        , m_cell_offsets(initial_particle_count)
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

        calculate_cell_offsets();
        sort_particles_into_cells();
        // TODO

        update_densities();
    }

    void sph_solver::set_particle_count(std::size_t particle_count)
    {
        m_particles.resize(particle_count);
        m_particle_indices.resize(particle_count);
        m_particle_histogram = std::vector<std::atomic_int>(particle_count);
        m_cell_offsets.resize(particle_count);
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
            if (particle.position.x < 0.f) {
                particle.position.x = 0.f;
                particle.velocity.x *= -1.f * (1.f - m_collision_dampening);
            }
            if (particle.position.x > m_simulation_area.x) {
                particle.position.x = m_simulation_area.x;
                particle.velocity.x *= -1.f * (1.f - m_collision_dampening);
            }
            if (particle.position.y < 0.f) {
                particle.position.y = 0.f;
                particle.velocity.y *= -1.f * (1.f - m_collision_dampening);
            }
            if (particle.position.y > m_simulation_area.y) {
                particle.position.y = m_simulation_area.y;
                particle.velocity.y *= -1.f * (1.f - m_collision_dampening);
            }

            particle.grid_index = grid_hash(grid_cell(particle.position)) % m_particles.size();
            m_particle_histogram[particle.grid_index] += 1;
        });
    }

    void sph_solver::calculate_cell_offsets()
    {
        constexpr std::size_t emulated_mp_count = 4;
        std::atomic_int global_cell_offset_count = 0;
        std::vector<std::atomic_int> local_particle_count(emulated_mp_count);
        std::vector<std::size_t> global_cell_offsets(emulated_mp_count);

        std::vector<std::unique_ptr<std::barrier<>>> inits_done;
        std::vector<std::size_t> barrier_sizes;

        auto threads_to_distribute = m_particle_histogram.size();
        for (std::size_t i = 0; i < emulated_mp_count; ++i) {
            auto mps_left = emulated_mp_count - i;
            auto threads_on_mp = (threads_to_distribute + mps_left - 1) / mps_left;
            inits_done.emplace_back(std::make_unique<std::barrier<>>(threads_on_mp));
            barrier_sizes.emplace_back(threads_on_mp);
            threads_to_distribute -= threads_on_mp;
        }

        auto enumerate_cells = utils::enumerate(m_particle_histogram);
        std::for_each(std::execution::par, std::begin(enumerate_cells), std::end(enumerate_cells),
                      [this, &global_cell_offset_count, &local_particle_count, &global_cell_offsets,
                       &inits_done](const auto& enumerated_cell_content_count) {
                          auto global_index = std::get<0>(enumerated_cell_content_count);
                          auto cell_content_count = std::get<1>(enumerated_cell_content_count).load();
                          auto local_index = global_index / emulated_mp_count;
                          auto emulated_mp_index = global_index % emulated_mp_count;

                          if (local_index == 0) { local_particle_count[emulated_mp_index] = 0; }

                          inits_done[emulated_mp_index]->arrive_and_wait();

                          auto local_particle_offset =
                              local_particle_count[emulated_mp_index].fetch_add(cell_content_count);

                          inits_done[emulated_mp_index]->arrive_and_wait();

                          if (local_index == 0) {
                              global_cell_offsets[emulated_mp_index] =
                                  global_cell_offset_count.fetch_add(local_particle_count[emulated_mp_index].load());
                          }

                          inits_done[emulated_mp_index]->arrive_and_wait();

                          auto cell_offset = global_cell_offsets[emulated_mp_index] + local_particle_offset;
                          m_cell_offsets[global_index] = cell_offset;
                          m_particle_histogram[global_index] = 0;
                      });
    }

    void sph_solver::sort_particles_into_cells()
    {
        auto enumerate_particles = utils::enumerate(m_particles);
        std::for_each(std::execution::par, std::begin(enumerate_particles), std::end(enumerate_particles),
                      [this](const auto& enumerated_particle) {
                          auto index = std::get<0>(enumerated_particle);
                          const auto& particle = std::get<1>(enumerated_particle);
                          auto sorted_index = m_cell_offsets[particle.grid_index]
                                              + m_particle_histogram[particle.grid_index].fetch_add(1);
                          m_particle_indices[sorted_index] = index;
                      });
    }

    void sph_solver::update_densities()
    {
        std::for_each(std::execution::seq, std::begin(m_particles), std::end(m_particles), [this](auto& particle) {
            particle.density = calculate_density(particle.position);
        });
    }

    glm::ivec2 sph_solver::grid_cell(const glm::vec2& p) const
    {
        return glm::uvec2{glm::floor(p / m_particle_radius)};
    }

    std::size_t sph_solver::grid_hash(const glm::ivec2& cell)
    {
        constexpr std::size_t prime_x = 94847;
        constexpr std::size_t prime_y = 31699;
        return cell.x * prime_x + cell.y * prime_y;
    }

    template<typename Ret, typename Pred>
    Ret sph_solver::accumulate_over_neighbourhood(const glm::vec2& p, const Ret& start_value, Pred predicate) const
    {
        Ret value = start_value;

        auto center_cell = grid_cell(p);
        for (int y_off = -1; y_off <= 1; ++y_off) {
            for (int x_off = -1; x_off <= 1; ++x_off) {
                auto current_grid_cell = center_cell + glm::ivec2{x_off, y_off};
                auto cell_hash = grid_hash(current_grid_cell) % m_particles.size();
                auto cell_size = static_cast<std::size_t>(m_particle_histogram[cell_hash].load());
                auto cell_offset = m_cell_offsets[cell_hash];
                auto cell_end = cell_offset + cell_size;

                for (auto particle_index = cell_offset; particle_index < cell_end; ++particle_index) {
                    const auto& particle = m_particles[m_particle_indices[particle_index]];
                    value += predicate(particle);
                }
            }
        }

        return value;
    }

    float sph_solver::calculate_density(const glm::vec2& p) const
    {
        return accumulate_over_neighbourhood(p, 0.f, [this, &p](const auto& particle) {
            float r = glm::length(particle.position - p);
            float influence = density_kernel(r);
            return m_particle_mass * influence;
        });
    }

    float sph_solver::calculate_property(const glm::vec2& p) const
    {
        return accumulate_over_neighbourhood(p, 0.f, [this, &p](const auto& particle) {
            float r = glm::length(particle.position - p);
            float influence = property_kernel(r);
            float density = particle.density;
            return particle.property * m_particle_mass * influence / density;
        });
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

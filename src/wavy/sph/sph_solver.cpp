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
#include <glm/gtc/random.hpp>
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
        simulate_gravity_and_predict_positions(delta_t);

        calculate_cell_offsets();
        sort_particles_into_cells();

        update_densities();
        apply_pressure(delta_t);
        update_positions_and_resolve_collisions(delta_t);
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
            std::mt19937 eng{seed};
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

    void sph_solver::simulate_gravity_and_predict_positions(float delta_t)
    {
        float delta_gravity = m_gravity * delta_t;
        std::for_each(std::execution::par, std::begin(m_particles), std::end(m_particles),
                      [this, delta_gravity, delta_t](auto& particle) {
                          particle.velocity += glm::vec2{0.f, -1.f} * delta_gravity;
                          particle.position_predicted = particle.position + particle.velocity * delta_t;

                          particle.grid_index = grid_hash(grid_cell(particle.position_predicted)) % m_particles.size();
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

        // TODO: there is a deadlock here.
        // deadlock is due to a limited number of threads all waiting for the barrier but it will never be filled.
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
        std::for_each(std::execution::par, std::begin(m_particles), std::end(m_particles),
                      [this](auto& particle) { particle.density = calculate_density(particle.position_predicted); });
    }

    void sph_solver::apply_pressure(float delta_t)
    {
        std::for_each(std::execution::par, std::begin(m_particle_indices), std::end(m_particle_indices),
                      [this, delta_t](auto particle_index) {
                          auto& particle = m_particles[particle_index];
                          auto pressure_force = calculate_pressure_force(particle_index);
                          auto pressure_acceleration = pressure_force / particle.density;
                          particle.velocity += pressure_acceleration * delta_t;
                      });
    }

    void sph_solver::update_positions_and_resolve_collisions(float delta_t)
    {
        std::for_each(std::execution::par, std::begin(m_particles), std::end(m_particles),
                      [this, delta_t](auto& particle) {
                          particle.position += particle.velocity * delta_t;
                          if (particle.position.x < 0.f) {
                              particle.position.x = 0.f;
                              particle.velocity.x *= -1.f * (1.f - m_config.get_collision_dampening());
                          }
                          if (particle.position.x > m_simulation_area.x) {
                              particle.position.x = m_simulation_area.x;
                              particle.velocity.x *= -1.f * (1.f - m_config.get_collision_dampening());
                          }
                          if (particle.position.y < 0.f) {
                              particle.position.y = 0.f;
                              particle.velocity.y *= -1.f * (1.f - m_config.get_collision_dampening());
                          }
                          if (particle.position.y > m_simulation_area.y) {
                              particle.position.y = m_simulation_area.y;
                              particle.velocity.y *= -1.f * (1.f - m_config.get_collision_dampening());
                          }
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
    Ret sph_solver::accumulate_over_neighbourhood(const glm::vec2& center_position, const Ret& start_value,
                                                  Pred predicate) const
    {
        Ret value = start_value;

        auto center_cell = grid_cell(center_position);
        for (int y_off = -1; y_off <= 1; ++y_off) {
            for (int x_off = -1; x_off <= 1; ++x_off) {
                auto current_grid_cell = center_cell + glm::ivec2{x_off, y_off};
                auto cell_hash = grid_hash(current_grid_cell) % m_particles.size();
                auto cell_size = static_cast<std::size_t>(m_particle_histogram[cell_hash].load());
                auto cell_offset = m_cell_offsets[cell_hash];
                auto cell_end = cell_offset + cell_size;

                for (auto particle_index = cell_offset; particle_index < cell_end; ++particle_index) {
                    value += predicate(m_particle_indices[particle_index]);
                }
            }
        }

        return value;
    }

    float sph_solver::calculate_density(const glm::vec2& center_position) const
    {
        return accumulate_over_neighbourhood(center_position, 0.f, [this, &center_position](auto particle_1_index) {
            return calculate_density_internal(center_position, particle_1_index);
        });
    }

    float sph_solver::calculate_property(const glm::vec2& center_position) const
    {
        return accumulate_over_neighbourhood(center_position, 0.f, [this, &center_position](auto particle_1_index) {
            return calculate_property_internal(center_position, particle_1_index);
        });
    }

    float sph_solver::calculate_property(std::size_t particle_index) const
    {
        const auto& center_position = m_particles[particle_index].position_predicted;
        return accumulate_over_neighbourhood(center_position, 0.f,
                                             [this, &center_position, &particle_index](auto particle_1_index) {
                                                 if (particle_index == particle_1_index) { return 0.f; }
                                                 return calculate_property_internal(center_position, particle_1_index);
                                             });
    }

    glm::vec2 sph_solver::calculate_property_gradient(const glm::vec2& center_position) const
    {
        return accumulate_over_neighbourhood(center_position, glm::vec2{0.f},
                                             [this, &center_position](auto particle_1_index) {
                                                 return calculate_property_internal(center_position, particle_1_index);
                                             });
    }

    glm::vec2 sph_solver::calculate_property_gradient(std::size_t particle_0_index) const
    {
        const auto& center_position = m_particles[particle_0_index].position_predicted;
        return accumulate_over_neighbourhood(
            center_position, glm::vec2{0.f}, [this, &center_position, &particle_0_index](auto particle_1_index) {
                if (particle_0_index == particle_1_index) { return glm::vec2{0.f}; }
                return calculate_property_gradient_internal(center_position, particle_1_index);
            });
    }

    glm::vec2 sph_solver::calculate_pressure_force(const glm::vec2& center_position) const
    {
        return accumulate_over_neighbourhood(
            center_position, glm::vec2{0.f}, [this, &center_position](auto particle_1_index) {
                return calculate_pressure_force_internal(center_position, particle_1_index);
            });
    }

    glm::vec2 sph_solver::calculate_pressure_force(std::size_t particle_0_index) const
    {
        const auto& center_position = m_particles[particle_0_index].position_predicted;
        return accumulate_over_neighbourhood(
            center_position, glm::vec2{0.f}, [this, &particle_0_index](auto particle_1_index) {
                if (particle_0_index == particle_1_index) { return glm::vec2{0.f}; }
                return calculate_pressure_force_internal(particle_0_index, particle_1_index);
            });
    }

    float sph_solver::density_kernel(float r) const
    {
        auto volume = glm::pi<float>() * glm::pow(m_config.get_particle_radius(), 8.f) / 4.f;
        auto value = glm::max(0.f, m_config.get_particle_radius() * m_config.get_particle_radius() - r * r);
        return value * value * value / volume;
    }

    float sph_solver::pressure_kernel(float r) const
    {
        if (r >= m_config.get_particle_radius()) { return 0.f; }
        auto volume = glm::pi<float>() * glm::pow(m_config.get_particle_radius(), 4.f) / 6.f;
        auto value = m_config.get_particle_radius() - r;
        return value * value / volume;
    }

    float sph_solver::pressure_kernel_derivative(float r) const
    {
        if (r >= m_config.get_particle_radius()) { return 0.f; }
        auto scale = -12.f / (glm::pi<float>() * glm::pow(m_config.get_particle_radius(), 4.f));
        auto value = m_config.get_particle_radius() - r;
        return value * scale;
    }

    float sph_solver::density_to_pressure(float density) const
    {
        auto density_error = density - m_config.get_target_density();
        return density_error * m_config.get_pressure_multiplier();
    }

    float sph_solver::calculate_shared_pressure(float particle_0_density, float particle_1_density) const
    {
        auto density_0 = density_to_pressure(particle_0_density);
        auto density_1 = density_to_pressure(particle_1_density);
        return .5f * (density_0 + density_1);
    }

    float sph_solver::calc_property(const glm::vec2& p)
    {
        return glm::cos(p.y - 3.f + glm::sin(p.x));
    }

    float sph_solver::calculate_density_internal(const glm::vec2& p, std::size_t particle_1_index) const
    {
        auto r = glm::length(m_particles[particle_1_index].position_predicted - p);
        auto influence = density_kernel(r);
        return m_config.get_particle_mass() * influence;
    }

    float sph_solver::calculate_property_internal(const glm::vec2& p, std::size_t particle_1_index) const
    {
        auto r = glm::length(m_particles[particle_1_index].position_predicted - p);
        auto influence = pressure_kernel(r);
        auto density = m_particles[particle_1_index].density;
        return m_particles[particle_1_index].property * m_config.get_particle_mass() * influence / density;
    }

    glm::vec2 sph_solver::calculate_property_gradient_internal(const glm::vec2& p, std::size_t particle_1_index) const
    {
        auto dir = m_particles[particle_1_index].position_predicted - p;
        auto r = glm::length(dir);
        dir = r == 0.f ? glm::circularRand(1.f) : dir / r;
        auto slope = pressure_kernel_derivative(r);
        auto density = m_particles[particle_1_index].density;
        return -m_particles[particle_1_index].property * dir * slope * m_config.get_particle_mass() / density;
    }

    glm::vec2 sph_solver::calculate_pressure_force_internal(std::size_t particle_0_index,
                                                            std::size_t particle_1_index) const
    {
        auto dir = m_particles[particle_1_index].position_predicted - m_particles[particle_0_index].position_predicted;
        auto r = glm::length(dir);
        dir = r == 0.f ? glm::circularRand(1.f) : dir / r;
        auto slope = pressure_kernel_derivative(r);
        auto density = m_particles[particle_1_index].density;
        auto shared_pressure = calculate_shared_pressure(m_particles[particle_0_index].density, density);
        return shared_pressure * dir * slope * m_config.get_particle_mass() / density;
    }

    glm::vec2 sph_solver::calculate_pressure_force_internal(const glm::vec2& center_position,
                                                            std::size_t particle_1_index) const
    {
        auto dir = m_particles[particle_1_index].position_predicted - center_position;
        auto r = glm::length(dir);
        dir = r == 0.f ? glm::circularRand(1.f) : dir / r;
        auto slope = pressure_kernel_derivative(r);
        auto density = m_particles[particle_1_index].density;
        return -density_to_pressure(density) * dir * slope * m_config.get_particle_mass() / density;
    }

}

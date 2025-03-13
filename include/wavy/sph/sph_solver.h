/**
 * @file   sph_solver.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief  Solver class for SPH(smooth particle hydrodynamics).
 */

#pragma once

#include <glm/vec2.hpp>
#include <vector>
#include <atomic>
#include <memory>

import sph;

namespace wavy::utils
{
    class compute_shader_emulator;
}

namespace wavy::sph {

    enum class particle_pattern
    {
        random,
        centered_grid
    };

    class sph_solver
    {
    public:
        explicit sph_solver(const glm::vec2& simulation_area, std::size_t initial_particle_count = 50,
                            particle_pattern pattern = particle_pattern::random);
        ~sph_solver();


        struct particle {
            particle() = default;
            explicit particle(const glm::vec2& p)
                : position{p}
                , position_predicted{p}
            {
            }
            glm::vec2 position = glm::vec2{0.f};
            glm::vec2 position_predicted = glm::vec2{0.f};
            glm::vec2 velocity = glm::vec2{0.f};
            float density = 0.f;
            float property = 0.f;
            std::size_t grid_index = static_cast<std::size_t>(-1);
        };

        struct external_influence
        {
            glm::vec2 position = glm::vec2{0.f};
            float radius = 0.f;
            float strength = 0.f;
        };

        void simulation_step(float delta_t, const external_influence* ext_influence);

        void set_particle_count(std::size_t particle_count);
        void set_particle_pattern(particle_pattern pattern);
        void reset_particles(unsigned int seed = 1337);

        const std::vector<particle>& get_particles() const { return m_particles; }
        particle_pattern get_pattern() const { return m_pattern; }
        const std::vector<std::atomic_int>& get_cell_sizes() const { return m_particle_histogram; }
        const std::vector<std::size_t>& get_cell_offsets() const { return m_cell_offsets; }
        const std::vector<std::size_t>& get_particle_indices() const { return m_particle_indices; }

        sph_config& get_config() { return m_config; }
        const sph_config& get_config() const { return m_config; }


        // public for visualization.
        float density_kernel(float r) const;
        float pressure_kernel(float r) const;
        float pressure_kernel_derivative(float r) const;
        float calculate_density(const glm::vec2& center_position) const;
        float calculate_property(const glm::vec2& center_position) const;
        glm::vec2 calculate_property_gradient(const glm::vec2& center_position) const;
        glm::vec2 calculate_pressure_force(const glm::vec2& center_position) const;
        glm::ivec2 grid_cell(const glm::vec2& p) const;
        static std::size_t grid_hash(const glm::ivec2& cell);

        auto get_current_external_influence() const { return m_current_ext_influence; }

    private:
        void simulate_gravity_and_predict_positions(float delta_t, const external_influence* ext_influence);
        void calculate_cell_offsets();
        void sort_particles_into_cells();
        void update_densities();
        void apply_pressure(float delta_t);
        void update_positions_and_resolve_collisions(float delta_t);

        template<typename Ret, typename Pred>
        Ret accumulate_over_neighbourhood(const glm::vec2& center_position, const Ret& start_value, Pred predicate) const;
        float calculate_property(std::size_t particle_index) const;
        glm::vec2 calculate_property_gradient(std::size_t particle_index) const;
        glm::vec2 calculate_pressure_force(std::size_t particle_index) const;

        float calculate_density_internal(const glm::vec2& p, std::size_t particle_1_index) const;
        float calculate_property_internal(const glm::vec2& p, std::size_t particle_1_index) const;
        glm::vec2 calculate_property_gradient_internal(const glm::vec2& p, std::size_t particle_1_index) const;
        glm::vec2 calculate_pressure_force_internal(std::size_t particle_0_index, std::size_t particle_1_index) const;
        glm::vec2 calculate_pressure_force_internal(const glm::vec2& center_position,
                                                    std::size_t particle_1_index) const;

        float density_to_pressure(float density) const;
        float calculate_shared_pressure(float particle_0_density, float particle_1_density) const;

        static float calc_property(const glm::vec2& p);

        glm::vec2 m_simulation_area;
        std::vector<particle> m_particles;
        std::vector<std::size_t> m_particle_indices;
        std::vector<std::atomic_int> m_particle_histogram;
        std::vector<std::size_t> m_cell_offsets;
        particle_pattern m_pattern;

        sph_config m_config;
        std::unique_ptr<utils::compute_shader_emulator> m_cs;

        const external_influence* m_current_ext_influence;
    };
}

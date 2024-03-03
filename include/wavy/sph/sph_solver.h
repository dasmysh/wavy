/**
 * @file   sph_solver.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.02.28
 *
 * @brief  Solver class for SPH(smooth particle hydrodynamics).
 */

#pragma once

#include <glm/vec2.hpp>

namespace wavy::sph {

    enum class particle_pattern
    {
        random,
        centered_grid
    };

    class sph_solver
    {
    public:

        explicit sph_solver(const glm::vec2& simulation_area, std::size_t initial_particle_count = 100,
                            particle_pattern pattern = particle_pattern::random);
        ~sph_solver();


        struct particle {
            particle() = default;
            explicit particle(const glm::vec2& p) : position{p} {}
            glm::vec2 position = glm::vec2{0.f};
            glm::vec2 velocity = glm::vec2{0.f};
            float density = 0.f;
            float property = 0.f;
            std::size_t grid_index = static_cast<std::size_t>(-1);
        };

        void simulation_step(float delta_t);

        void set_particle_count(std::size_t particle_count);
        void set_particle_pattern(particle_pattern pattern);
        void reset_particles(unsigned int seed = 1337);

        const std::vector<particle>& get_particles() const { return m_particles; }
        particle_pattern get_pattern() const { return m_pattern; }
        const std::vector<std::atomic_int>& get_cell_sizes() const { return m_particle_histogram; }
        const std::vector<std::size_t>& get_cell_offsets() const { return m_cell_offsets; }
        const std::vector<std::size_t>& get_particle_indices() const { return m_particle_indices; }

        float get_particle_radius() const { return m_particle_radius; }
        void set_particle_radius(float radius) { m_particle_radius = radius; }

        float get_particle_mass() const { return m_particle_mass; }
        void set_particle_mass(float mass) { m_particle_mass = mass; }

        float get_gravity() const { return m_gravity; }
        void set_gravity(float gravity) { m_gravity = gravity; }

        float get_collision_dampening() const { return m_collision_dampening; }
        void set_collision_dampening(float collision_dampening) { m_collision_dampening = collision_dampening; }

        // public for visualization.
        float density_kernel(float r) const;
        float property_kernel(float r) const;
        float calculate_density(const glm::vec2& p) const;
        float calculate_property(const glm::vec2& p) const;
        glm::ivec2 grid_cell(const glm::vec2& p) const;
        static std::size_t grid_hash(const glm::ivec2& cell);

    private:
        void simulate_gravity(float delta_t);
        void resolve_collisions();
        void calculate_cell_offsets();
        void sort_particles_into_cells();
        void update_densities();

        template<typename Ret, typename Pred>
        Ret accumulate_over_neighbourhood(const glm::vec2& p, const Ret& start_value, Pred predicate) const;


        static float calc_property(const glm::vec2& p);

        glm::vec2 m_simulation_area;
        std::vector<particle> m_particles;
        std::vector<std::size_t> m_particle_indices;
        std::vector<std::atomic_int> m_particle_histogram;
        std::vector<std::size_t> m_cell_offsets;
        particle_pattern m_pattern;
        float m_particle_radius = 20.f;
        float m_particle_mass = 1.f;

        float m_gravity = 0.f;// 9.81f;
        float m_collision_dampening = .1f;
    };
}

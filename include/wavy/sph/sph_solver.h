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
            glm::vec2 position;
        };

        void set_particle_count(std::size_t particle_count);
        void set_particle_pattern(particle_pattern pattern);
        void reset_particles(unsigned int seed = 1337);

        const std::vector<particle>& get_particles() const { return m_particles; }
        particle_pattern get_pattern() const { return m_pattern; }

    private:
        glm::vec2 m_simulation_area;
        std::vector<particle> m_particles;
        particle_pattern m_pattern;
    };
}

/**
 * @file   sph_config.cxx
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.06
 *
 * @brief  Module for SPH configuration objects.
 */

module;

export module sph;// : config;

namespace wavy::sph
{
    export class sph_config
    {
    public:
        sph_config() = default;
        float get_particle_radius() const { return m_particle_radius; }
        void set_particle_radius(float radius) { m_particle_radius = radius; }

        float get_particle_mass() const { return m_particle_mass; }
        void set_particle_mass(float mass) { m_particle_mass = mass; }

        float get_gravity() const { return m_gravity; }
        void set_gravity(float gravity) { m_gravity = gravity; }

        float get_collision_dampening() const { return m_collision_dampening; }
        void set_collision_dampening(float collision_dampening) { m_collision_dampening = collision_dampening; }

        float get_target_density() const { return m_target_density; }
        void set_target_density(float target_density) { m_target_density = target_density; }

        float get_pressure_multiplier() const { return m_pressure_multiplier; }
        void set_pressure_multiplier(float pressure_multiplier) { m_pressure_multiplier = pressure_multiplier; }
    private:
        float m_particle_radius = 20.f;
        float m_particle_mass = 1.f;
        float m_target_density = 2.75f;
        float m_pressure_multiplier = .5f;

        float m_gravity = 0.f; // 9.81f;
        float m_collision_dampening = .1f;
    };
}

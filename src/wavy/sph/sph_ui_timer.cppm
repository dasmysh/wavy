/**
 * @file   sph_ui_timer.cppm
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.20
 *
 * @brief  Module for the SPH simulation timer.
 */

module;

export module wavy.sphui:timer;
import :gui;

namespace wavy::sph {
    export class sph_timer
    {
    public:
        sph_timer() = default;

        void update_time(sph_solver& solver, sph_gui& gui, float delta_t);

        float get_delta_t() const { return m_last_delta_t; }
        bool is_delta_t_out_of_bounds() const { return m_delta_t_out_of_bounds; }

    private:
        float m_last_delta_t = 0.016f;
        bool m_delta_t_out_of_bounds = false;
    };
}

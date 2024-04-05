/**
 * @file   compute_shader_cpu_emulation.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.13
 *
 * @brief  Implementation of compute shader emulation on the CPU.
 */

#include "utils/compute_shader_cpu_emulation.h"

namespace wavy::utils {
    coro::task<> compute_shader_emulator::resume_on_thread_pool(
        coro::thread_pool& tp, std::shared_ptr<async_barrier> scheduling_finsied_barrier, coro::task<>& kernel) const
    {
        auto coroutine = kernel.handle();
        co_await tp.schedule();
        coroutine.resume();
        scheduling_finsied_barrier->count_down();
    }
}

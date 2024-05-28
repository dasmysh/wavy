/**
 * @file   compute_shader_cpu_emulation.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.13
 *
 * @brief  Implementation of compute shader emulation on the CPU.
 */

#include "utils/compute_shader_cpu_emulation.h"

#include <coro/generator.hpp>

namespace wavy::utils {
    std::size_t logged_task_counter::s_counter = 0;

    synced_task<> compute_shader_emulator::resume_on_worker_thread_pool(task<>& kernel)
    {
        co_await resume_on_thread_pool(m_worker_thread_pool, kernel);
        co_return;
    }

    synced_task<> compute_shader_emulator::resume_on_work_groups_thread_pool(task<>& kernel)
    {
        co_await resume_on_thread_pool(m_work_groups_thread_pool, kernel);
        co_return;
    }

    task<> compute_shader_emulator::resume_on_thread_pool(coro::thread_pool& tp, task<>& kernel) const
    {
        auto coroutine = kernel.handle();
        co_await tp.schedule();
        coroutine.resume();
    }
}

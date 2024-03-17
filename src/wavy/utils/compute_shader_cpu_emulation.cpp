/**
 * @file   compute_shader_cpu_emulation.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.13
 *
 * @brief  Implementation of compute shader emulation on the CPU.
 */

#include "utils/compute_shader_cpu_emulation.h"

namespace wavy::utils {

    cppcoro::task<> resume_on_thread_pool(cppcoro::static_thread_pool& tp,
                                                             std::shared_ptr<async_barrier> scheduling_finsied_barrier,
                                                             const cppcoro::task<>& kernel)
    {
        auto coroutine = kernel.when_ready().m_coroutine;
        co_await tp.schedule();
        coroutine.resume();
        scheduling_finsied_barrier->count_down();
    }

    cppcoro::task<> wait_for_emulated_cs_tasks(std::vector<cppcoro::task<>>&& awaitables)
    {
        co_await cppcoro::when_all(std::move(awaitables));
    }

}

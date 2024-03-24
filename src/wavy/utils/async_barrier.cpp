/**
 * @file   async_barrier.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.11
 *
 * @brief  Implementation of co-routine helper classes.
 */

#include "utils/async_barrier.h"
#include <algorithm>

namespace wavy::utils {

    async_barrier::async_barrier(std::ptrdiff_t initial_count) noexcept
        : m_count{initial_count}
        , m_barrier_hit{initial_count <= 0}
        , m_initial_count{initial_count}
    {
    }

    void async_barrier::reset() noexcept
    {
        m_count.store(m_initial_count);
        m_barrier_hit.store(m_initial_count <= 0);
    }

    void async_barrier::reset(std::ptrdiff_t new_initial_count) noexcept
    {
        m_initial_count = new_initial_count;
        reset();
    }

    void async_barrier::count_down(std::ptrdiff_t n /*= 1*/) noexcept
    {
        if (m_count.fetch_sub(n, std::memory_order_acq_rel) <= n) { m_barrier_hit.store(true); }
    }

    bool async_barrier::barrier_scheduling_awaiter::await_ready() const noexcept
    {
        return barrier.m_barrier_hit.load();
    }

    bool async_barrier::barrier_scheduling_awaiter::await_suspend(std::coroutine_handle<>) const noexcept
    {
        return !barrier.m_barrier_hit.load();
    }
}

/**
 * @file   async_barrier_on_threadpool.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.11
 *
 * @brief  Implementation of co-routine helper classes.
 */

#include "utils/async_barrier_on_threadpool.h"
#include <algorithm>

namespace wavy::utils {

    async_barrier_on_threadpool::async_barrier_on_threadpool(std::ptrdiff_t initial_count) noexcept
        : m_count{initial_count}
        , m_barrier_hit{initial_count <= 0}
        , m_initial_count{initial_count}
    {
    }

    void async_barrier_on_threadpool::reset(std::ptrdiff_t initial_count) noexcept
    {
        m_count.store(initial_count);
        m_barrier_hit.store(initial_count <= 0);
        m_initial_count = initial_count;
    }

    void async_barrier_on_threadpool::reset() noexcept
    {
        m_count.store(m_initial_count);
        m_barrier_hit.store(m_initial_count <= 0);
    }

    void async_barrier_on_threadpool::count_down(std::ptrdiff_t n /*= 1*/) noexcept
    {
        if (m_count.fetch_sub(n, std::memory_order_acq_rel) <= n) { m_barrier_hit.store(true); }
    }

    bool async_barrier_on_threadpool::barrier_scheduling_awaiter::await_ready() const noexcept
    {
        return barrier.m_barrier_hit.load();
    }

    bool
    async_barrier_on_threadpool::barrier_scheduling_awaiter::await_suspend(std::coroutine_handle<>) noexcept
    {
        return !barrier.m_barrier_hit.load();
    }
}

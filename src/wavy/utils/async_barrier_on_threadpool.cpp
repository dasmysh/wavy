/**
 * @file   async_barrier_on_threadpool.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.11
 *
 * @brief  Implementation of co-routine helper classes.
 */

#include "utils/async_barrier_on_threadpool.h"

namespace wavy::utils {

    async_barrier_on_threadpool::async_barrier_on_threadpool() noexcept
        : m_count{0}
        , m_event{nullptr}
    {
    }

    async_barrier_on_threadpool::async_barrier_on_threadpool(std::shared_ptr<cppcoro::static_thread_pool> tp,
                                                             std::ptrdiff_t initial_count) noexcept
        : m_count{initial_count}
        , m_event{tp, initial_count <= 0}
        , m_resume_tasks{static_cast<std::size_t>(initial_count) - 1}
    {
    }

    void async_barrier_on_threadpool::reset(std::shared_ptr<cppcoro::static_thread_pool> tp,
                                          std::ptrdiff_t initial_count) noexcept
    {
        std::scoped_lock lock{m_resume_tasks_mutex};
        m_count.store(initial_count);
        m_event.reset(tp);
        m_resume_tasks.resize(static_cast<std::size_t>(initial_count) - 1);
    }

    void async_barrier_on_threadpool::count_down(std::ptrdiff_t n /*= 1*/) noexcept
    {
        if (m_count.fetch_sub(n, std::memory_order_acq_rel) <= n) {
            std::scoped_lock lock{m_resume_tasks_mutex};
            m_count.store(m_resume_tasks.size() + 1);
            m_event.set(m_resume_tasks);

            for (const auto& task : m_resume_tasks) { task.when_ready().m_coroutine.resume(); }
        }
    }
}

/**
 * @file   async_barrier_on_threadpool.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.11
 *
 * @brief  Helper classes for co-routines.
 */

#pragma once

#include "utils/async_manual_reset_event_on_threadpool.h"

namespace wavy::utils {

    class async_barrier_on_threadpool
    {
    public:
        async_barrier_on_threadpool() noexcept;
        async_barrier_on_threadpool(std::shared_ptr<cppcoro::static_thread_pool> tp,
                                    std::ptrdiff_t initial_count) noexcept;
        ~async_barrier_on_threadpool() = default;

        void reset(std::shared_ptr<cppcoro::static_thread_pool> tp, std::ptrdiff_t initial_count) noexcept;
        bool is_ready() const noexcept { return m_event.is_set(); }
        void count_down(std::ptrdiff_t n = 1) noexcept;
        auto operator co_await() noexcept
        {
            count_down();
            return m_event.operator co_await();
        }

    private:
        std::atomic<std::ptrdiff_t> m_count;
        async_manual_reset_event_on_threadpool m_event;
        std::vector<cppcoro::task<>> m_resume_tasks;
        std::mutex m_resume_tasks_mutex;
    };
}

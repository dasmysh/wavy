/**
 * @file   async_barrier.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.11
 *
 * @brief  Helper classes for co-routines.
 */

#pragma once

#include <atomic>
#include <coroutine>

namespace wavy::utils {

    class async_barrier
    {
    public:
        async_barrier() noexcept = default;
        explicit async_barrier(std::ptrdiff_t initial_count) noexcept;
        ~async_barrier() = default;

        void reset() noexcept;
        bool is_ready() const noexcept { return m_barrier_hit.load(); }
        void count_down(std::ptrdiff_t n = 1) noexcept;

        auto operator co_await() noexcept
        {
            count_down();
            return std::suspend_always{};
        }

        struct barrier_scheduling_awaiter
        {
            explicit barrier_scheduling_awaiter(async_barrier& b)
                : barrier{b}
            {}

            auto operator co_await() const noexcept { return *this; }

            bool await_ready() const noexcept;
            bool await_suspend(std::coroutine_handle<> awaiter) const noexcept;
            void await_resume() const noexcept {}

            async_barrier& barrier;
        };

        auto scheduling() noexcept { return barrier_scheduling_awaiter{*this}; }

    private:
        std::atomic<std::ptrdiff_t> m_count = 0;
        std::atomic_bool m_barrier_hit = true;
        std::ptrdiff_t m_initial_count = 0;
    };
}

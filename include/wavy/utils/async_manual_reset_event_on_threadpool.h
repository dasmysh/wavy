/**
 * @file   async_manual_reset_event_on_threadpool.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.11
 *
 * @brief  Asynchronous manual reset event on a thread pool.
 */

#pragma once

#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>
#include <coroutine>
#include <atomic>

namespace wavy::utils {
    class async_manual_reset_event_operation_on_threadpool;

    /// <summary>
    /// see cppcoro/async_manual_reset_event.hpp for documentation.
    /// This class works basically the same but schedules all resuming
    /// coroutines on a thread pool.
    /// </summary>
    class async_manual_reset_event_on_threadpool
    {
    public:
        async_manual_reset_event_on_threadpool(std::shared_ptr<cppcoro::static_thread_pool> tp,
                                               bool initiallySet = false) noexcept;
        ~async_manual_reset_event_on_threadpool();

        async_manual_reset_event_operation_on_threadpool operator co_await() const noexcept;

        bool is_set() const noexcept;
        void set(std::vector<cppcoro::task<>>& resume_tasks) noexcept;
        void reset() noexcept;
        void reset(std::shared_ptr<cppcoro::static_thread_pool> tp) noexcept;

    private:
        friend class async_manual_reset_event_operation_on_threadpool;

        std::shared_ptr<cppcoro::static_thread_pool> m_thread_pool;
        mutable std::atomic<void*> m_state;
    };


	class async_manual_reset_event_operation_on_threadpool
    {
    public:
        explicit async_manual_reset_event_operation_on_threadpool(
            const async_manual_reset_event_on_threadpool& event) noexcept;

        bool await_ready() const noexcept;
        bool await_suspend(std::coroutine_handle<> awaiter) noexcept;
        void await_resume() const noexcept {}

    private:
        friend class async_manual_reset_event_on_threadpool;

        const async_manual_reset_event_on_threadpool& m_event;
        async_manual_reset_event_operation_on_threadpool* m_next = nullptr;
        std::coroutine_handle<> m_awaiter;
    };
}

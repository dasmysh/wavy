/**
 * @file   async_manual_reset_event_on_threadpool.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.11
 *
 * @brief  Implementation of the asynchronous manual reset event on a thread pool.
 */

#include "utils/async_manual_reset_event_on_threadpool.h"

namespace wavy::utils {

    async_manual_reset_event_on_threadpool::async_manual_reset_event_on_threadpool(
        std::shared_ptr<cppcoro::static_thread_pool> tp, bool initiallySet /*= false*/) noexcept
        : m_thread_pool{tp}
        , m_state(initiallySet ? static_cast<void*>(this) : nullptr)
    {
    }

    async_manual_reset_event_on_threadpool::~async_manual_reset_event_on_threadpool()
    {
        assert(m_state.load(std::memory_order_relaxed) == nullptr
               || m_state.load(std::memory_order_relaxed) == static_cast<void*>(this));
    }

    bool async_manual_reset_event_on_threadpool::is_set() const noexcept
    {
        return m_state.load(std::memory_order_acquire) == static_cast<const void*>(this);
    }

    async_manual_reset_event_operation_on_threadpool
        async_manual_reset_event_on_threadpool::operator co_await() const noexcept
    {
        return async_manual_reset_event_operation_on_threadpool{*this};
    }

    void async_manual_reset_event_on_threadpool::set(std::vector<cppcoro::task<>>& resume_tasks) noexcept
    {
        auto const setState = static_cast<void*>(this);

        auto oldState = m_state.exchange(setState, std::memory_order_acq_rel);
        if (oldState != setState) {
            auto* current = static_cast<async_manual_reset_event_operation_on_threadpool*>(oldState);

            std::size_t task_index = 0;
            while (current != nullptr) {
                auto* next = current->m_next;

                resume_tasks[task_index] = [](auto tp, auto awaiter) -> cppcoro::task<> {
                    co_await tp->schedule();
                    awaiter.resume();
                }(m_thread_pool, current->m_awaiter);

                current = next;
                task_index += 1;
            }

            assert(task_index == resume_tasks.size());
        }
    }

    void async_manual_reset_event_on_threadpool::reset() noexcept
    {
        auto oldState = static_cast<void*>(this);
        m_state.compare_exchange_strong(oldState, nullptr, std::memory_order_relaxed);
    }

    void async_manual_reset_event_on_threadpool::reset(std::shared_ptr<cppcoro::static_thread_pool> tp) noexcept
    {
        m_thread_pool = tp;
        m_state.store(nullptr);
    }

    async_manual_reset_event_operation_on_threadpool::async_manual_reset_event_operation_on_threadpool(
        const async_manual_reset_event_on_threadpool& event) noexcept
        : m_event(event)
    {
    }

    bool async_manual_reset_event_operation_on_threadpool::await_ready() const noexcept
    {
        return m_event.is_set();
    }

    bool async_manual_reset_event_operation_on_threadpool::await_suspend(std::coroutine_handle<> awaiter) noexcept
    {
        m_awaiter = awaiter;

        auto setState = static_cast<const void*>(&m_event);

        void* oldState = m_event.m_state.load(std::memory_order_acquire);
        do {
            if (oldState == setState) { return false; }

            m_next = static_cast<async_manual_reset_event_operation_on_threadpool*>(oldState);
        } while (!m_event.m_state.compare_exchange_weak(oldState, static_cast<void*>(this), std::memory_order_release,
                                                        std::memory_order_acquire));
        return true;
    }
}

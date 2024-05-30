/**
 * @file   synced_task.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.05.27
 *
 * @brief  Internal task type that is synchronized with an async_barrier.
 */

#pragma once

#include "async_barrier.h"

#include <coroutine>
#include <variant>
#include <spdlog/spdlog.h>

namespace wavy::utils {
    namespace detail {

        struct unset_return_value
        {
            unset_return_value() = default;
            unset_return_value(unset_return_value&&) = delete;
            unset_return_value(const unset_return_value&) = delete;
            auto operator=(unset_return_value&&) = delete;
            auto operator=(const unset_return_value&) = delete;
        };

        class synced_task_promise_base
        {
        public:
            synced_task_promise_base() noexcept = default;

            auto initial_suspend() const noexcept -> std::suspend_always { return {}; }

            auto unhandled_exception() -> void { m_exception = std::current_exception(); }

        protected:
            ~synced_task_promise_base() = default;
            async_barrier& get_barrier() { return *m_barrier; }
            void set_barrier(async_barrier& barrier) { m_barrier = &barrier; }

            const std::exception_ptr& get_exception() { return m_exception; }

        private:
            async_barrier* m_barrier = nullptr;
            std::exception_ptr m_exception;
        };

        template<typename return_type> class synced_task_promise : public synced_task_promise_base
        {
        public:
            using coroutine_type = std::coroutine_handle<synced_task_promise<return_type>>;

            static constexpr bool return_type_is_reference = std::is_reference_v<return_type>;
            using stored_type = std::conditional_t<return_type_is_reference, std::remove_reference_t<return_type>*,
                                                   std::remove_const_t<return_type>>;
            using variant_type = std::variant<unset_return_value, stored_type, std::exception_ptr>;

            synced_task_promise() noexcept = default;
            synced_task_promise(const synced_task_promise&) = delete;
            synced_task_promise(synced_task_promise&&) = delete;
            auto operator=(const synced_task_promise&) -> synced_task_promise& = delete;
            auto operator=(synced_task_promise&&) -> synced_task_promise& = delete;
            ~synced_task_promise() = default;

            auto start(async_barrier& barrier)
            {
                m_barrier = &barrier;
                coroutine_type::from_promise(*this).resume();
            }

            auto get_return_object() noexcept { return coroutine_type::from_promise(*this); }

            template<typename value_type>
                requires(return_type_is_reference and std::is_constructible_v<return_type, value_type &&>)
                        or (not return_type_is_reference and std::is_constructible_v<stored_type, value_type &&>)
            auto return_value(value_type&& value) -> void
            {
                if constexpr (return_type_is_reference) {
                    return_type ref = static_cast<value_type&&>(value);
                    m_storage.template emplace<stored_type>(std::addressof(ref));
                } else {
                    m_storage.template emplace<stored_type>(std::forward<value_type>(value));
                }
            }

            auto return_value(stored_type value) -> void
                requires(not return_type_is_reference)
            {
                if constexpr (std::is_move_constructible_v<stored_type>) {
                    m_storage.template emplace<stored_type>(std::move(value));
                } else {
                    m_storage.template emplace<stored_type>(value);
                }
            }

            auto final_suspend() noexcept
            {
                struct completion_notifier
                {
                    auto await_ready() const noexcept { return false; }
                    auto await_suspend(coroutine_type coroutine) const noexcept { coroutine.promise().m_event->set(); }
                    auto await_resume() noexcept {};
                };

                return completion_notifier{};
            }

            auto result() & -> decltype(auto)
            {
                if (std::holds_alternative<stored_type>(m_storage)) {
                    if constexpr (return_type_is_reference) {
                        return static_cast<return_type>(*std::get<stored_type>(m_storage));
                    } else {
                        return static_cast<const return_type&>(std::get<stored_type>(m_storage));
                    }
                } else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
                    std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
                } else {
                    throw std::runtime_error{"The return value was never set, did you execute the coroutine?"};
                }
            }

            auto result() const& -> decltype(auto)
            {
                if (std::holds_alternative<stored_type>(m_storage)) {
                    if constexpr (return_type_is_reference) {
                        return static_cast<std::add_const_t<return_type>>(*std::get<stored_type>(m_storage));
                    } else {
                        return static_cast<const return_type&>(std::get<stored_type>(m_storage));
                    }
                } else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
                    std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
                } else {
                    throw std::runtime_error{"The return value was never set, did you execute the coroutine?"};
                }
            }

            auto result() && -> decltype(auto)
            {
                if (std::holds_alternative<stored_type>(m_storage)) {
                    if constexpr (return_type_is_reference) {
                        return static_cast<return_type>(*std::get<stored_type>(m_storage));
                    } else if constexpr (std::is_assignable_v<return_type, stored_type>) {
                        return static_cast<return_type&&>(std::get<stored_type>(m_storage));
                    } else {
                        return static_cast<const return_type&&>(std::get<stored_type>(m_storage));
                    }
                } else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
                    std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
                } else {
                    throw std::runtime_error{"The return value was never set, did you execute the coroutine?"};
                }
            }

        private:
            variant_type m_storage{};
        };

        template<> class synced_task_promise<void> : public synced_task_promise_base
        {
            using coroutine_type = std::coroutine_handle<synced_task_promise<void>>;

        public:
            synced_task_promise() noexcept = default;
            ~synced_task_promise() = default;

            auto start(async_barrier& barrier)
            {
                set_barrier(barrier);
                coroutine_type::from_promise(*this).resume();
            }

            auto get_return_object() noexcept { return coroutine_type::from_promise(*this); }

            auto final_suspend() noexcept
            {
                struct completion_notifier
                {
                    auto await_ready() const noexcept { return false; }
                    auto await_suspend(coroutine_type coroutine) const noexcept
                    {
                        coroutine.promise().get_barrier().count_down();
                    }
                    auto await_resume() noexcept {};
                };

                return completion_notifier{};
            }

            auto return_void() const noexcept -> void {}

            auto result() -> void
            {
                if (get_exception()) { std::rethrow_exception(get_exception()); }
            }
        };
    }

    template<typename return_type = void> class synced_task
    {
    public:
        using promise_type = detail::synced_task_promise<return_type>;
        using coroutine_type = std::coroutine_handle<promise_type>;

        synced_task() noexcept = default;
        synced_task(coroutine_type coroutine) noexcept
            : m_coroutine(coroutine)
        {
            spdlog::info("+synced_task {}", m_coroutine.address());
        }

        synced_task(const synced_task&) = delete;
        synced_task(synced_task&& other) noexcept
            : m_coroutine(std::exchange(other.m_coroutine, coroutine_type{}))
        {
        }
        auto operator=(const synced_task&) -> synced_task& = delete;
        auto operator=(synced_task&& other) noexcept -> synced_task&
        {
            if (std::addressof(other) != this) { m_coroutine = std::exchange(other.m_coroutine, coroutine_type{}); }

            return *this;
        }

        ~synced_task()
        {
            spdlog::info("-synced_task {}", m_coroutine.address());
            if (m_coroutine) { m_coroutine.destroy(); }
        }

        auto is_ready() const noexcept -> bool { return m_coroutine == nullptr || m_coroutine.done(); }

        void start(async_barrier& barrier)
        {
            m_coroutine.promise().start(barrier);
        }

        auto promise() & -> promise_type& { return m_coroutine.promise(); }
        auto promise() const& -> const promise_type& { return m_coroutine.promise(); }
        auto promise() && -> promise_type&& { return std::move(m_coroutine.promise()); }

    private:
        coroutine_type m_coroutine = nullptr;
    };
}

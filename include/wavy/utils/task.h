/**
 * @file   task.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.04.21
 *
 * @brief  Internal task type to be able to optionally log some activity.
 */

#pragma once

#include <coro/task.hpp>
#include <spdlog/spdlog.h>

namespace wavy::utils {

    template<typename return_type = void> class logged_task;

    namespace detail {
        struct logged_promise_base
        {
            friend struct final_awaitable;
            struct final_awaitable
            {
                auto await_ready() const noexcept -> bool { return false; }

                template<typename promise_type>
                auto await_suspend(std::coroutine_handle<promise_type> coroutine) const noexcept
                    -> std::coroutine_handle<>
                {
                    // If there is a continuation call it, otherwise this is the end of the line.
                    auto& promise = coroutine.promise();
                    if (promise.m_continuation != nullptr) {
                        return promise.m_continuation;
                    } else {
                        return std::noop_coroutine();
                    }
                }

                auto await_resume() const noexcept -> void
                {
                    // no-op
                }
            };

            logged_promise_base() noexcept = default;
            ~logged_promise_base() = default;

            auto initial_suspend() const noexcept { return std::suspend_always{}; }
            auto final_suspend() const noexcept { return final_awaitable{}; }
            auto continuation(std::coroutine_handle<> continuation) noexcept -> void { m_continuation = continuation; }

        private:
            std::coroutine_handle<> m_continuation{nullptr};
        };

        template<typename return_type> struct logged_promise final : public logged_promise_base
        {
        private:
            struct unset_return_value
            {
                unset_return_value() = default;
                unset_return_value(unset_return_value&&) = delete;
                unset_return_value(const unset_return_value&) = delete;
                auto operator=(unset_return_value&&) = delete;
                auto operator=(const unset_return_value&) = delete;
            };

        public:
            using task_type = logged_task<return_type>;
            using coroutine_handle = std::coroutine_handle<logged_promise<return_type>>;
            static constexpr bool return_type_is_reference = std::is_reference_v<return_type>;
            using stored_type = std::conditional_t<return_type_is_reference, std::remove_reference_t<return_type>*,
                                                   std::remove_const_t<return_type>>;
            using variant_type = std::variant<unset_return_value, stored_type, std::exception_ptr>;

            logged_promise() noexcept = default;
            logged_promise(const logged_promise&) = delete;
            logged_promise(logged_promise&& other) = delete;
            logged_promise& operator=(const logged_promise&) = delete;
            logged_promise& operator=(logged_promise&& other) = delete;
            ~logged_promise() = default;

            auto get_return_object() noexcept -> task_type;

            template<typename value_type>
                requires(return_type_is_reference && std::is_constructible_v<return_type, value_type &&>)
                        || (!return_type_is_reference && std::is_constructible_v<stored_type, value_type &&>)
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
                requires(!return_type_is_reference)
            {
                if constexpr (std::is_move_constructible_v<stored_type>) {
                    m_storage.template emplace<stored_type>(std::move(value));
                } else {
                    m_storage.template emplace<stored_type>(value);
                }
            }

            auto unhandled_exception() noexcept -> void { new (&m_storage) variant_type(std::current_exception()); }

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
                    } else if constexpr (std::is_move_constructible_v<return_type>) {
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

        template<> struct logged_promise<void> : public logged_promise_base
        {
            using task_type = logged_task<void>;
            using coroutine_handle = std::coroutine_handle<logged_promise<void>>;

            logged_promise() noexcept = default;
            logged_promise(const logged_promise&) = delete;
            logged_promise(logged_promise&& other) = delete;
            logged_promise& operator=(const logged_promise&) = delete;
            logged_promise& operator=(logged_promise&& other) = delete;
            ~logged_promise() = default;

            auto get_return_object() noexcept -> task_type;
            auto return_void() noexcept -> void {}
            auto unhandled_exception() noexcept -> void { m_exception_ptr = std::current_exception(); }
            auto result() -> void
            {
                if (m_exception_ptr) { std::rethrow_exception(m_exception_ptr); }
            }

        private:
            std::exception_ptr m_exception_ptr{nullptr};
        };
    }

    class logged_task_counter
    {
    public:
        auto get_id() const { return m_id; }

    private:
        static std::size_t s_counter;
        std::size_t m_id = s_counter++;
    };

    template<typename return_type>
    class [[nodiscard("Coroutine  would be useless if this was discarded")]] logged_task
        : public logged_task_counter
    {
    public:
        using task_type = logged_task<return_type>;
        using promise_type = detail::logged_promise<return_type>;
        using coroutine_handle = std::coroutine_handle<promise_type>;

        struct awaitable_base
        {
            explicit awaitable_base(coroutine_handle coroutine) noexcept
                : m_coroutine(coroutine)
            {
            }

            auto await_ready() const noexcept -> bool { return !m_coroutine || m_coroutine.done(); }

            auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept -> std::coroutine_handle<>
            {
                m_coroutine.promise().continuation(awaiting_coroutine);
                return m_coroutine;
            }

            std::coroutine_handle<promise_type> m_coroutine{nullptr};
        };

        logged_task() noexcept { spdlog::error("empty logged task (id: {}) created.", get_id()); }

        explicit logged_task(coroutine_handle handle)
            : m_coroutine(handle)
        {
            spdlog::error("logged task (id: {}, handle: {}) created.", get_id(), m_coroutine.address());
        }
        logged_task(const logged_task&) = delete;
        logged_task(logged_task&& other) noexcept
            : m_coroutine(std::exchange(other.m_coroutine, nullptr))
        {
            spdlog::error("logged task (id: {}, handle: {}) moved.", get_id(), m_coroutine.address());
        }

        ~logged_task()
        {
            spdlog::error("logged task (id: {}, handle: {}) destroyed.", get_id(), m_coroutine.address());
            if (m_coroutine != nullptr) { m_coroutine.destroy(); }
        }

        auto operator=(const logged_task&) -> logged_task& = delete;

        auto operator=(logged_task&& other) noexcept -> logged_task&
        {
            if (std::addressof(other) != this) {
                if (m_coroutine != nullptr) { m_coroutine.destroy(); }

                m_coroutine = std::exchange(other.m_coroutine, nullptr);
            }

            return *this;
        }

        auto is_ready() const noexcept -> bool { return m_coroutine == nullptr || m_coroutine.done(); }

        auto resume() -> bool
        {
            if (!m_coroutine.done()) { m_coroutine.resume(); }
            return !m_coroutine.done();
        }

        auto destroy() -> bool
        {
            if (m_coroutine != nullptr) {
                m_coroutine.destroy();
                m_coroutine = nullptr;
                return true;
            }

            return false;
        }

        auto operator co_await() const& noexcept
        {
            struct awaitable : public awaitable_base
            {
                auto await_resume() -> decltype(auto) { return this->m_coroutine.promise().result(); }
            };

            return awaitable{m_coroutine};
        }

        auto operator co_await() const&& noexcept
        {
            struct awaitable : public awaitable_base
            {
                auto await_resume() -> decltype(auto) { return std::move(this->m_coroutine.promise()).result(); }
            };

            return awaitable{m_coroutine};
        }

        auto promise() & -> promise_type& { return m_coroutine.promise(); }
        auto promise() const& -> const promise_type& { return m_coroutine.promise(); }
        auto promise() && -> promise_type&& { return std::move(m_coroutine.promise()); }

        auto handle() -> coroutine_handle { return m_coroutine; }

    private:
        coroutine_handle m_coroutine{nullptr};
    };

    template<class return_type = void> using task = coro::task<return_type>;
    // template<class return_type = void> using task = logged_task<return_type>;
}

namespace wavy::utils::detail {
    template<typename return_type>
    inline auto logged_promise<return_type>::get_return_object() noexcept -> logged_task<return_type>
    {
        return task<return_type>{coroutine_handle::from_promise(*this)};
    }

    inline auto logged_promise<void>::get_return_object() noexcept -> logged_task<>
    {
        return logged_task<>{coroutine_handle::from_promise(*this)};
    }
}

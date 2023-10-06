/**
 * @file   owned_ptr.h
 * @author Sebastian Maisch <sebastian.maisch@uni-ulm.de>
 * @date   2016.05.08
 *
 * @brief  Definition of the owned pointer.
 */

#pragma once

#include <utility>

namespace mysh::core::stdext {

    template <typename T> class owned_ptr final
    {
    public:
        owned_ptr() : m_ptr{ nullptr }, m_owned{ false } {};
        owned_ptr(T* ptr, bool owned) : m_ptr{ ptr }, m_owned{ owned } {};
        owned_ptr(const owned_ptr& rhs) = delete;
        owned_ptr& operator=(const owned_ptr&) = delete;
        owned_ptr(owned_ptr&& rhs) noexcept : m_ptr(rhs.ptr_), m_owned{ rhs.owned_ } { rhs.ptr_ = nullptr; }
        owned_ptr& operator=(owned_ptr&& rhs) noexcept { m_ptr = rhs.ptr_; m_owned = rhs.owned_; rhs.ptr_ = nullptr; return *this; }
        ~owned_ptr() { if (owned_) delete m_ptr; }

        operator T*() const { return m_ptr; }
        T* operator->() { return m_ptr; }
        const T* operator->() const { return m_ptr; }
        T& operator*() { return *ptr_; }
        const T& operator*() const { return *ptr_; }
        explicit operator bool() const { return nullptr != m_ptr; }
        bool operator==(const owned_ptr& rhs) { return rhs.ptr_ == m_ptr; }

        T* release() { T* tmp = m_ptr; m_ptr = nullptr; return tmp; }
        void reset(T* newObj = nullptr) { this->~owned_ptr(); m_ptr = newObj; }
        void swap(owned_ptr& other) noexcept { T* tmp = m_ptr; m_ptr = other.ptr_; other.ptr_ = tmp; }
        bool is_owned() const { return m_owned; }

    private:
        /** Holds the pointer. */
        T* ptr_;
        /** Holds a flag whether the pointer is owned or not. */
        bool owned_;
    };

    template<class T, class... Args> owned_ptr<T> make_owned(Args&&... args)
    {
        return owned_ptr<T>(new T(std::forward<Args>(args)...), true);
    }
}

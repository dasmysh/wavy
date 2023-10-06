/**
 * @file   aligned_vector.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2017.04.17
 *
 * @brief  Definition of a vector class that allows its entries to be aligned at compile time.
 */

#pragma once

#include <vector>
#include <cstdint>

namespace mysh::core {

    template<typename T> class aligned_vector
    {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using reference = value_type&;
        using const_reference = const value_type&;

        aligned_vector(size_type alignedSize, size_type count, const T& value);
        explicit aligned_vector(size_type alignedSize, size_type count = 0) : m_alignedSize{alignedSize}
        {
            m_cont.resize(count * alignedSize);
        }
        // template<class InputIt> aligned_vector(size_type alignedSize, InputIt first, InputIt last);
        aligned_vector(size_type alignedSize, std::initializer_list<T> init);
        aligned_vector(const aligned_vector& rhs) : m_alignedSize{rhs.m_alignedSize}, m_cont{rhs.m_cont} {}
        aligned_vector& operator=(const aligned_vector&);
        aligned_vector(aligned_vector&& rhs) noexcept : m_alignedSize{rhs.m_alignedSize}, m_cont{std::move(rhs.m_cont)}
        {
        }
        aligned_vector& operator=(aligned_vector&&) noexcept;
        ~aligned_vector();

        /*aligned_vector& operator=(std::initializer_list<T> ilist);

        void assign(size_type count, const T& value);
        template<class InputIt> void assign(size_type alignment, InputIt first, InputIt last);
        void assign(std::initializer_list<T> init);*/

        reference at(size_type pos) { return *reinterpret_cast<T*>(&m_cont.at(pos * m_alignedSize)); }
        const_reference at(size_type pos) const { return *reinterpret_cast<const T*>(&m_cont.at(pos * m_alignedSize)); }
        reference operator[](size_type pos) { return *reinterpret_cast<T*>(&m_cont[pos * m_alignedSize]); }
        const_reference operator[](size_type pos) const
        {
            return *reinterpret_cast<const T*>(&m_cont[pos * m_alignedSize]);
        }
        reference front() { return *reinterpret_cast<T*>(&m_cont.front()); }
        const_reference front() const { return *reinterpret_cast<const T*>(&m_cont.front()); }
        reference back() { return *reinterpret_cast<T*>(&m_cont[m_cont.size() - m_alignedSize]); }
        const_reference back() const { return *reinterpret_cast<const T*>(&m_cont[m_cont.size() - m_alignedSize]); }
        T* data() noexcept { return reinterpret_cast<T*>(m_cont.data()); }
        const T* data() const noexcept { return reinterpret_cast<const T*>(m_cont.data()); }

        /*iterator begin() noexcept;
        const_iterator begin() const noexcept;
        const_iterator cbegin() const noexcept;
        iterator end() noexcept;
        const_iterator end() const noexcept;
        const_iterator cend() const noexcept;
        reverse_iterator rbegin() noexcept;
        const_reverse_iterator rbegin() const noexcept;
        const_reverse_iterator crbegin() const noexcept;
        reverse_iterator rend() noexcept;
        const_reverse_iterator rend() const noexcept;
        const_reverse_iterator crend() const noexcept;*/

        [[nodiscard]] bool empty() const noexcept { return m_cont.empty(); }
        [[nodiscard]] size_type size() const noexcept { return m_cont.size() / m_alignedSize; }
        [[nodiscard]] size_type max_size() const noexcept { return m_cont.max_size() / m_alignedSize; }
        void reserve(size_type new_cap) { m_cont.reserve(new_cap * m_alignedSize); }
        [[nodiscard]] size_type capacity() const noexcept { return m_cont.capacity() / m_alignedSize; }
        void shrink_to_fit() { m_cont.shrink_to_fit(); }

        void clear() noexcept { m_cont.clear(); }
        /*iterator insert(const_iterator pos, const T& value);
        iterator insert(const_iterator pos, T&& value);
        iterator insert(const_iterator pos, size_type count, const T& value);
        template<class InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last);
        iterator insert(const_iterator pos, std::initializer_list<T> ilist);
        template<class... Args> iterator emplace(const_iterator pos, Args&&... args);
        iterator erase(const_iterator pos);
        iterator erase(const_iterator first, const_iterator last);*/
        void push_back(const T& value);
        void push_back(T&& value);
        template<class... Args> reference emplace_back(Args&&... args);
        void pop_back();
        void resize(size_type count) { m_cont.resize(count * m_alignedSize); }
        void resize(size_type count, const value_type& value);
        void swap(aligned_vector& other) noexcept;

        [[nodiscard]] std::size_t GetAlignedSize() const { return m_alignedSize; }

    private:
        /** Holds the vectors alignment. */
        std::size_t m_alignedSize;
        /** Holds the internal byte vector. */
        std::vector<std::uint8_t> m_cont;
    };

    template<typename T>
    inline aligned_vector<T>::aligned_vector(size_type alignedSize, size_type count, const T& value) :
        aligned_vector{ alignedSize, count }
    {
        for (size_type i = 0U; i < count; ++i) { new (reinterpret_cast<T*>(m_cont[i * m_alignedSize])) T(value); }
    }

    template<typename T>
    inline aligned_vector<T>::aligned_vector(size_type alignedSize, std::initializer_list<T> init) :
        aligned_vector{ alignedSize, init.size() }
    {
        size_type i = 0U;
        for (const auto& elem : init) {
            new(reinterpret_cast<T*>(m_cont[i * m_alignedSize])) T(elem);
            i += 1;
        }
    }

    template<typename T>
    inline aligned_vector<T>& aligned_vector<T>::operator=(const aligned_vector<T>& rhs) // NOLINT
    {
        if (this != &rhs) {
            m_alignedSize = rhs.m_alignedSize;
            m_cont = rhs.m_cont;
        }
        return *this;
    }

    template<typename T>
    inline aligned_vector<T>& aligned_vector<T>::operator=(aligned_vector<T>&& rhs) noexcept
    {
        m_alignedSize = rhs.m_alignedSize;
        m_cont = std::move(rhs.m_cont);
        return *this;
    }

    template<typename T>
    inline aligned_vector<T>::~aligned_vector()
    {
        for (size_type i = 0; i < m_cont.size(); i += m_alignedSize) reinterpret_cast<T*>(&m_cont[i])->~T();
    }

    template<typename T>
    inline void aligned_vector<T>::push_back(const T& value)
    {
        auto oldSize = m_cont.size();
        m_cont.resize(oldSize + m_alignedSize);
        new (reinterpret_cast<T*>(m_cont.data() + oldSize)) T(value);
    }

    template<typename T>
    inline void aligned_vector<T>::push_back(T&& value)
    {
        auto oldSize = m_cont.size();
        m_cont.resize(oldSize + m_alignedSize);
        new (reinterpret_cast<T*>(m_cont.data() + oldSize)) T(std::move(value));
    }

    template<typename T>
    template<class ...Args>
    inline typename aligned_vector<T>::reference aligned_vector<T>::emplace_back(Args&&... args)
    {
        auto oldSize = m_cont.size();
        m_cont.resize(oldSize + m_alignedSize);
        new (reinterpret_cast<T*>(m_cont.data() + oldSize)) T(std::forward<Args>(args)...);

        return back();
    }

    template<typename T>
    inline void aligned_vector<T>::pop_back()
    {
        for (auto i = 0U; i < m_alignedSize; ++i) m_cont.pop_back();
    }

    template<typename T>
    inline void aligned_vector<T>::resize(size_type count, const value_type& value)
    {
        auto oldElems = m_cont.size() / m_alignedSize;
        m_cont.resize(count * m_alignedSize);
        for (auto i = oldElems; i < count; ++i) {
            auto newElemPos = (oldElems + i) * m_alignedSize;
            new (reinterpret_cast<T*>(m_cont.data() + newElemPos)) T(value);
        }
    }

    template<typename T>
    inline void aligned_vector<T>::swap(aligned_vector& other) noexcept
    {
        auto tmp = other.m_alignedSize;
        other.m_alignedSize = m_alignedSize;
        m_alignedSize = tmp;
        m_cont.swap(other.m_cont);
    }
}

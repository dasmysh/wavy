/**
 * @file   zip.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.13
 *
 * @brief  Python like zip implementation (adjusted from https://committhis.github.io/2020/10/14/zip-iterator.html)
 */

#pragma once

#include "iterator_helpers.h"

#include <tuple>

namespace fluid::utils
{
    namespace zip_detail
    {
        template<std::forward_iterator... Iters> class zip_iterator;
        template<std::ranges::range... Ts> class zip_wrapper;

        template<typename T>
        concept NonZipIterator =
            !requires(T a) { []<typename... Ts>(zip_iterator<Ts...>&) { /* empty */ }(a); };

        template<typename T>
        concept NonZipWrapperRange =
            std::ranges::range<T> && !requires(T a) { []<typename... Ts>(zip_wrapper<Ts...>&) { /* empty */ }(a); };

        template<std::forward_iterator... Iters>
        class zip_iterator
        {
        public:
            using iterator_tuple = std::tuple<Iters ...>;
            using difference_type = std::ptrdiff_t;
            using value_type = std::tuple<select_access_type_for<Iters>...>;
            using reference = std::tuple<typename Iters::reference ...>;
            using iterator_category = std::forward_iterator_tag;

            zip_iterator() = default;

            template<NonZipIterator... Args>
            explicit zip_iterator(Args&&... iters)
                : m_iterators{std::forward<Args>(iters)...}
            {
            }

            bool operator==(const zip_iterator& other) const { return any_match(m_iterators, other.m_iterators); }

            zip_iterator& operator++()
            {
                std::apply([](auto&... iterators) { ((++iterators), ...); }, m_iterators);
                return *this;
            }

            zip_iterator operator++(int)
            {
                auto result = *this;
                ++*this;
                return result;
            }

            reference operator*() const
            {
                return std::apply([](auto&&... iterators) { return reference(*iterators...); }, m_iterators);
            }

        private:
            iterator_tuple m_iterators;
        };

        template<std::ranges::range... Ts>
        class zip_wrapper
        {
        public:
            using const_iterator = zip_iterator<select_iterator_for<const Ts> ...>;
            using iterator = zip_iterator<select_iterator_for<Ts> ...>;

            template<NonZipWrapperRange... Args>
            explicit zip_wrapper(Args&&... iterables)
                : m_iterables{std::forward<Args>(iterables)...}
            {
            }

            auto begin()
            {
                return std::apply([](auto&&... iterable) { return iterator(std::begin(iterable)...); },
                                  m_iterables);
            }
            auto end()
            {
                return std::apply([](auto&&... iterable) { return iterator(std::end(iterable)...); },
                                  m_iterables);
            }

        private:
            std::tuple<Ts...> m_iterables;
        };
    }

    template<std::ranges::range... Ts> constexpr auto zip(Ts&& ... iterables)
    {
        return zip_detail::zip_wrapper<Ts...>{std::forward<Ts>(iterables)...};
    }
}

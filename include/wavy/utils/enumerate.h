/**
 * @file   enumerate.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.09
 *
 * @brief  Python like enumerate implementation (adjusted from https://www.reedbeta.com/blog/python-like-enumerate-in-cpp17/)
 */

#pragma once

#include "iterator_helpers.h"

#include <tuple>

namespace fluid::utils
{
    namespace enum_detail
    {
        template<std::forward_iterator iterator_type>
        class iterator
        {
            using counter_type = std::size_t;
            counter_type i{};
            iterator_type iter;

        public:
            using difference_type = std::ptrdiff_t;
            using value_type = std::tuple<counter_type, typename iterator_type::value_type>;
            using reference = std::tuple<counter_type, typename iterator_type::reference>;
            using iterator_category = std::forward_iterator_tag;

            iterator() = default;

            explicit iterator(iterator_type it)
                : iter(it)
            {
            }

            bool operator==(const iterator& other) const { return this->iter == other.iter; }

            iterator& operator++()
            {
                ++i;
                ++iter;
                return *this;
            }

            iterator operator++(int)
            {
                auto result = *this;
                ++*this;
                return result;
            }

            auto operator*() const { return reference{i, *iter}; }
            auto& operator->() const { return *iter; }
        };
    }

    template<std::ranges::range T> constexpr auto enumerate(T&& iterable)
    {
        struct enumerate_wrapper
        {
            T iterable;

            using const_iterator = enum_detail::iterator<select_iterator_for<const T>>;
            using iterator = enum_detail::iterator<select_iterator_for<T>>;

            auto begin() { return enum_detail::iterator<select_iterator_for<T>>(std::begin(iterable)); }
            auto end() { return enum_detail::iterator<select_iterator_for<T>>(std::end(iterable)); }
        };

        return enumerate_wrapper{std::forward<T>(iterable)};
    }
}

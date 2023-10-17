/**
 * @file   iterator_helpers.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.13
 *
 * @brief  Some helper templates for custom iterators.
 */

#pragma once

#include <ranges>
#include <vector>

namespace wavy::utils
{
    template<typename T>
    using select_iterator_for =
        std::conditional_t<std::is_const_v<std::remove_reference_t<T>>, typename std::decay_t<T>::const_iterator,
                           typename std::decay_t<T>::iterator>;

    template<typename Iter>
    using select_access_type_for = std::conditional_t<
        std::is_same_v<Iter, std::vector<bool>::iterator> || std::is_same_v<Iter, std::vector<bool>::const_iterator>,
        typename std::iterator_traits<Iter>::value_type, typename std::iterator_traits<Iter>::reference>;

    /*  The index sequence is only used to deduce the Index sequence in the template
    declaration. It uses a fold expression which is applied to the indexes,
    using each expanded value to compare tuple value at that index. If any of
    the tuple elements are equal, the function will return true. */
    template<typename... Args, std::size_t... Index>
    auto any_match_impl(std::tuple<Args...> const& lhs, std::tuple<Args...> const& rhs, std::index_sequence<Index...>)
        -> bool
    {
        return (... | (std::get<Index>(lhs) == std::get<Index>(rhs)));
    }

    /*  User function for finding any elementwise match in two tuples. Forwards to
    to the implementation the two tuples along with a generated index sequence
    which will have the same length as the tuples. */
    template<typename... Args> auto any_match(std::tuple<Args...> const& lhs, std::tuple<Args...> const& rhs) -> bool
    {
        return any_match_impl(lhs, rhs, std::index_sequence_for<Args...>{});
    }
}

/**
 * @file   type_traits.h
 * @author Sebastian Maisch <sebastian.maisch@uni-ulm.de>
 * @date   2016.05.08
 *
 * @brief  Definition of some type traits used.
 */

#pragma  once

#include "core/aligned_vector.h"

#include <vector>
#include <array>
#include <concepts>
#include <type_traits>

namespace mysh::core {

    template<typename T>
    struct has_contiguous_memory : std::false_type {};

    template<typename T, typename U>
    struct has_contiguous_memory<std::vector<T, U>> : std::true_type{};

    template<typename T>
    struct has_contiguous_memory<std::vector<bool, T>> : std::false_type{};

    template<typename T, typename U, typename V>
    struct has_contiguous_memory<std::basic_string<T, U, V>> : std::true_type{};

    template<typename T, std::size_t N>
    struct has_contiguous_memory<std::array<T, N>> : std::true_type{};

    template<typename T>
    struct has_contiguous_memory<T[]> : std::true_type{}; // NOLINT

    template<typename T, std::size_t N>
    struct has_contiguous_memory<T[N]> : std::true_type{}; // NOLINT

    template<typename T>
    struct has_contiguous_memory<aligned_vector<T>> : std::true_type {};


    template<typename T> concept contiguous_memory = requires
    {
        std::derived_from<has_contiguous_memory<T>, std::true_type>;
    };


    template<contiguous_memory T> std::size_t byteSizeOf(const T& data)
    {
        return static_cast<std::size_t>(sizeof(typename T::value_type) * data.size());
    }

    template<class T> std::size_t byteSizeOf(const aligned_vector<T>& data) {
        return static_cast<std::size_t>(data.GetAlignedSize() * data.size());
    }
}

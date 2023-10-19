/**
 * @file   test_aligned_vector.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.18
 *
 * @brief  Tests for the aligned vector helper class.
 */

#include "core/aligned_vector.h"

#include <catch.hpp>

namespace mysh::core
{
    TEST_CASE("mysh::core::aligned_vector.general", "")
    {
        constexpr std::size_t alignment = 8;
        constexpr std::size_t element_count = 20;
        aligned_vector av(alignment, element_count, 1.0f);
        REQUIRE((&av[1] - av.data()) * sizeof(float) == alignment);
    }
}

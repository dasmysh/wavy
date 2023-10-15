#include "utils/enumerate.h"
#include "utils/zip.h"

#include <catch.hpp>
#include <numeric>
#include <vector>

TEST_CASE("enumerate", "[enumerate]")
{
    std::vector<std::size_t> v;
    v.resize(50);
    std::iota(std::begin(v), std::end(v), 0);

    for (const auto& [index, value] : fluid::utils::enumerate(v))
    {
        REQUIRE(static_cast<std::size_t>(index) == value);
    }
}

TEST_CASE("zip", "[zip]")
{
    std::vector<std::size_t> v0;
    std::vector<std::size_t> v1;
    v0.resize(50);
    v1.resize(50);
    std::iota(std::begin(v0), std::end(v0), 0);
    std::iota(std::begin(v1), std::end(v1), 10);

    auto zipper = fluid::utils::zip(v0, v1);

    for (const auto& [value0, value1] : zipper) { REQUIRE(value0 + 10 == value1); }
}

TEST_CASE("copy_constructor", "[zip]")
{
    std::vector<std::size_t> v0;
    std::vector<std::size_t> v1;
    v0.resize(50);
    v1.resize(50);
    std::iota(std::begin(v0), std::end(v0), 0);
    std::iota(std::begin(v1), std::end(v1), 10);

    auto zipper = fluid::utils::zip(v0, v1);
    auto zipper2{zipper};
    auto zipper3{std::move(zipper)};

    for (const auto& [value0, value1] : zipper2) { REQUIRE(value0 + 10 == value1); }
    for (const auto& [value0, value1] : zipper3) { REQUIRE(value0 + 10 == value1); }
}

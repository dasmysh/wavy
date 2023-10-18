#include "utils/enumerate.h"
#include "utils/zip.h"

#include <catch.hpp>
#include <execution>
#include <numeric>
#include <vector>

namespace wavy::utils
{
    class zip_fixture
    {
    protected:
        zip_fixture()
        {
            v0.resize(vector0_size);
            v1.resize(vector1_size);
            std::iota(std::begin(v0), std::end(v0), 0);
            std::iota(std::begin(v1), std::end(v1), 10);
        }

        auto zip_v() { return zip(v0, v1); }
        auto zip_nested_v() { return zip(enumerate(v0), v1); }

        constexpr static std::size_t test_constant = 30;

    private:
        constexpr static std::size_t vector0_size = 50;
        constexpr static std::size_t vector1_size = 60;

        std::vector<std::size_t> v0;
        std::vector<std::size_t> v1;
    };

    TEST_CASE_METHOD(zip_fixture , "wavy::utils.zip.range based for loop", "[zip][range_based_for]")
    {
        for (const auto& [value0, value1] : zip_v()) { REQUIRE(value0 + 10 == value1); }

        for (auto [value0, value1] : zip_v()) { value1 = value0 + 5; }

        for (const auto& [value0, value1] : zip_v()) { REQUIRE(value0 + 5 == value1); }
    }

    TEST_CASE_METHOD(zip_fixture, "wavy::utils.zip.for_each with std_par", "[zip][std::for_each]")
    {
        auto zipper = zip_v();
        std::for_each(std::execution::par, std::begin(zipper), std::end(zipper), [](auto v_element) {
            auto& value0 = std::get<0>(v_element);
            auto& value1 = std::get<1>(v_element);
            REQUIRE(value0 + 10 == value1);

            value1 = value0 + 5;
        });

        std::for_each(std::execution::par, std::begin(zipper), std::end(zipper), [](auto v_element) {
            auto& value0 = std::get<0>(v_element);
            auto& value1 = std::get<1>(v_element);
            REQUIRE(value0 + 5 == value1);
        });
    }

    TEST_CASE_METHOD(zip_fixture, "wavy::utils.zip.copy and move constructors", "[zip][range_based_for]")
    {
        constexpr std::size_t test_constant2 = 99;
        auto zipper = zip_v();
        for (auto zipper2{zipper}; auto [value0, value1] : zipper2) {
            REQUIRE(value0 + 10 == value1);
            value1 = value0 + test_constant2;
        }

        for (auto zipper3{std::move(zipper)}; const auto& [value0, value1] : zipper3) {
            REQUIRE(value0 + test_constant2 == value1);
        }
    }

    TEST_CASE_METHOD(zip_fixture, "wavy::utils.zip.copy and move constructors with parallel for_each",
                     "[enumerate][std::for_each]")
    {
        auto zipper = zip_v();
        auto zipper2{zipper};
        auto zipper3{std::move(zipper)};

        std::for_each(std::execution::par, std::begin(zipper2), std::end(zipper2), [](auto v_element) {
            auto& value0 = std::get<0>(v_element);
            auto& value1 = std::get<1>(v_element);
            REQUIRE(value0 + 10 == value1);
            value0 = 3 * value1;
        });

        std::for_each(std::execution::par, std::begin(zipper3), std::end(zipper3), [](auto v_element) {
            auto& value0 = std::get<0>(v_element);
            auto& value1 = std::get<1>(v_element);
            REQUIRE(value0 == value1 * 3);
        });
    }

    TEST_CASE_METHOD(zip_fixture, "wavy::utils.zip.nested enumerate", "[enumerate][nested][range_based_for]")
    {
        for (auto [value0, value1] : zip_nested_v()) {
            auto index = std::get<0>(value0);
            REQUIRE(index == std::get<1>(value0));
            REQUIRE(index + 10 == value1);

            std::get<1>(value0) = index * test_constant;
        }

        for (const auto& [value0, value1] : zip_nested_v()) {
            REQUIRE(std::get<0>(value0) * test_constant == std::get<1>(value0));
        }
    }

    TEST_CASE_METHOD(zip_fixture, "wavy::utils.zip.nested enumerate with parallel for_each",
                     "[enumerate][nested][std::for_each]")
    {
        {
            auto zipper = zip_nested_v();
            std::for_each(std::execution::par, std::begin(zipper), std::end(zipper), [](auto v_element) {
                const std::size_t index = std::get<0>(std::get<0>(v_element));
                auto& value0 = std::get<1>(std::get<0>(v_element));
                auto& value1 = std::get<1>(v_element);

                REQUIRE(index == value0);
                REQUIRE(index + 10 == value1);

                value1 = index * test_constant;
            });
        }

        {
            auto zipper = zip_nested_v();
            std::for_each(std::execution::par, std::begin(zipper), std::end(zipper), [](auto v_element) {
                const std::size_t index = std::get<0>(std::get<0>(v_element));
                auto& value1 = std::get<1>(v_element);

                REQUIRE(index * test_constant == value1);
            });
        }
    }
}

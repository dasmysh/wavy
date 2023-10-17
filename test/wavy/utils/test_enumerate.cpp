#include "utils/enumerate.h"
#include "utils/zip.h"

#include <catch.hpp>
#include <execution>
#include <numeric>
#include <vector>

namespace fluid::utils
{
    class enumerate_fixture
    {
    protected:
        enumerate_fixture()
        {
            v.resize(50);
            std::iota(std::begin(v), std::end(v), 0);
        }

        auto enumerate_v() { return enumerate(v); }

    private:
        std::vector<std::size_t> v;
    };

    class enumerate_fixture_nested
    {
    protected:
        enumerate_fixture_nested()
        {
            v0.resize(50);
            std::iota(std::begin(v0), std::end(v0), 0);

            v1.resize(50);
            std::iota(std::begin(v1), std::end(v1), 10);
        }

        auto enumerate_v() { return enumerate(zip(v0, v1)); }

    private:
        std::vector<std::size_t> v0;
        std::vector<std::size_t> v1;
    };

    TEST_CASE_METHOD(enumerate_fixture, "fluid::utils::enumerate.range based for loop", "[enumerate][range_based_for]")
    {
        for (const auto& [index, value] : enumerate_v()) { REQUIRE(index == value); }

        for (auto [index, value] : enumerate_v()) { value = index + 5; }

        for (const auto& [index, value] : enumerate_v()) { REQUIRE(index + 5 == value); }
    }

    TEST_CASE_METHOD(enumerate_fixture, "fluid::utils::enumerate.for_each with std_par", "[enumerate][std::for_each]")
    {
        auto enum_v = enumerate_v();
        std::for_each(std::execution::par, std::begin(enum_v), std::end(enum_v), [](auto v_element) {
            std::size_t index = std::get<0>(v_element);
            auto& value = std::get<1>(v_element);
            REQUIRE(index == value);

            value = index + 5;
        });

        std::for_each(std::execution::par, std::begin(enum_v), std::end(enum_v), [](auto v_element) {
            std::size_t index = std::get<0>(v_element);
            auto& value = std::get<1>(v_element);
            REQUIRE(index + 5 == value);
        });
    }

    TEST_CASE_METHOD(enumerate_fixture, "fluid::utils.enumerate.copy and move constructors", "[enumerate][range_based_for]")
    {
        auto enum_wrapper = enumerate_v();
        for (auto enum_wrapper2{enum_wrapper}; auto [index, value] : enum_wrapper2) {
            REQUIRE(index == value);
            value = index + 99;
        }

        auto enum_wrapper3{std::move(enum_wrapper)};
        for (const auto& [index, value] : enum_wrapper3) { REQUIRE(index + 99 == value); }
    }

    TEST_CASE_METHOD(enumerate_fixture, "fluid::utils.enumerate.copy and move constructors with parallel for_each", "[enumerate][std::for_each]")
    {
        auto enum_wrapper = enumerate_v();
        auto enum_wrapper2{enum_wrapper};
        auto enum_wrapper3{std::move(enum_wrapper)};

        std::for_each(std::execution::par, std::begin(enum_wrapper2), std::end(enum_wrapper2), [](auto v_element) {
            std::size_t index = std::get<0>(v_element);
            auto& value = std::get<1>(v_element);
            REQUIRE(index == value);
            value = 3 * index;
        });


        std::for_each(std::execution::par, std::begin(enum_wrapper3), std::end(enum_wrapper3), [](auto v_element) {
            std::size_t index = std::get<0>(v_element);
            auto& value = std::get<1>(v_element);
            REQUIRE(index * 3 == value);
        });
    }

    TEST_CASE_METHOD(enumerate_fixture_nested, "fluid::utils.enumerate.nested zip",
                     "[enumerate][nested][range_based_for]")
    {
        for (auto [index, value] : enumerate_v()) {
            REQUIRE(index == std::get<0>(value));
            REQUIRE(index + 10 == std::get<1>(value));

            std::get<1>(value) = index * 30;
        }

        for (const auto& [index, value] : enumerate_v()) {
            REQUIRE(index * 30 == std::get<1>(value));
        }
    }

    TEST_CASE_METHOD(enumerate_fixture_nested, "fluid::utils.enumerate.nested zip with parallel for_each",
                     "[enumerate][nested][std::for_each]")
    {
        {
            auto enum_wrapper = enumerate_v();
            std::for_each(std::execution::par, std::begin(enum_wrapper), std::end(enum_wrapper), [](auto v_element) {
                std::size_t index = std::get<0>(v_element);
                auto& value = std::get<1>(v_element);

                REQUIRE(index == std::get<0>(value));
                REQUIRE(index + 10 == std::get<1>(value));

                std::get<1>(value) = index * 30;
            });
        }

        {
            auto enum_wrapper = enumerate_v();
            std::for_each(std::execution::par, std::begin(enum_wrapper), std::end(enum_wrapper), [](auto v_element) {
                std::size_t index = std::get<0>(v_element);
                auto& value = std::get<1>(v_element);

                REQUIRE(index * 30 == std::get<1>(value));
            });
        }
    }
}

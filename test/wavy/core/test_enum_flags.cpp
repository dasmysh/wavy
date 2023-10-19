/**
 * @file   test_enum_flags.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.18
 *
 * @brief  Tests for the enum flags helper class.
 */

#include "core/enum_flags.h"

#include <catch.hpp>

namespace mysh::core
{
    enum class TestEnumFlagBits : std::uint8_t
    {
        BIT0 = 0x1,
        BIT1 = 0x2,
        BIT2 = 0x4
    };

    template<> struct EnableBitMaskOperators<TestEnumFlagBits>
    {
        static constexpr bool enable = true;
    };

    TEST_CASE("mysh::core::enum_flags.general", "")
    {
        using TestEnumFlags = EnumFlags<TestEnumFlagBits>;

        TestEnumFlags ef;

        ef |= TestEnumFlagBits::BIT0;
        REQUIRE(ef & TestEnumFlagBits::BIT0);
        ef |= TestEnumFlagBits::BIT2;
        REQUIRE(ef & TestEnumFlagBits::BIT0);
        REQUIRE(ef & TestEnumFlagBits::BIT2);
        REQUIRE(!(ef & TestEnumFlagBits::BIT1));
        ef &= ~TestEnumFlags{TestEnumFlagBits::BIT0};
        REQUIRE(ef & TestEnumFlagBits::BIT2);
        REQUIRE(!(ef & TestEnumFlagBits::BIT0));
        REQUIRE(!(ef & TestEnumFlagBits::BIT1));
    }
}

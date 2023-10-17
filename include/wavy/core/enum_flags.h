/**
 * @file   enum_flags.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2017.03.23
 *
 * @brief  Implementation of a template for enum flags.
 */

#pragma once

#include <type_traits>

namespace mysh::core {

    template<typename Enum> struct EnableBitMaskOperators { static constexpr bool enable = false; };
    template<typename Enum, typename Enable = void> class EnumFlags;

    template<typename Enum> class EnumFlags<Enum, typename std::enable_if<EnableBitMaskOperators<Enum>::enable>::type>
    {
    public:
        using base_type = typename std::underlying_type<Enum>::type;

        EnumFlags() : m_mask(static_cast<base_type>(0)) {}
        EnumFlags(Enum bit) : m_mask(static_cast<base_type>(bit)) {} // NOLINT
        EnumFlags(base_type flags) : m_mask{flags} {} // NOLINT
        EnumFlags(const EnumFlags&) = default;
        EnumFlags(EnumFlags&&) noexcept = default;
        EnumFlags& operator=(const EnumFlags&) = default;
        EnumFlags& operator=(EnumFlags&&) noexcept = default;
        ~EnumFlags() = default;

        EnumFlags<Enum>& operator|=(const EnumFlags<Enum>& rhs) { m_mask |= rhs.m_mask; return *this; } // NOLINT(hicpp-signed-bitwise)
        EnumFlags<Enum>& operator&=(const EnumFlags<Enum>& rhs) { m_mask &= rhs.m_mask; return *this; } // NOLINT(hicpp-signed-bitwise)
        EnumFlags<Enum>& operator^=(const EnumFlags<Enum>& rhs) { m_mask ^= rhs.m_mask; return *this; } // NOLINT(hicpp-signed-bitwise)
        EnumFlags<Enum> operator|(const EnumFlags<Enum>& rhs) const { EnumFlags<Enum> result(*this); result |= rhs; return result; } // NOLINT(hicpp-signed-bitwise)
        EnumFlags<Enum> operator&(const EnumFlags<Enum>& rhs) const { EnumFlags<Enum> result(*this); result &= rhs; return result; } // NOLINT(hicpp-signed-bitwise)
        EnumFlags<Enum> operator^(const EnumFlags<Enum>& rhs) const { EnumFlags<Enum> result(*this); result ^= rhs; return result; } // NOLINT(hicpp-signed-bitwise)
        bool operator!() const { return !m_mask; }
        EnumFlags<Enum> operator~() const { return ~m_mask; }
        bool operator==(const EnumFlags<Enum>& rhs) const { return m_mask == rhs.m_mask; }
        bool operator!=(const EnumFlags<Enum>& rhs) const { return m_mask != rhs.m_mask; }
        explicit operator bool() const { return !!m_mask; }
        explicit operator base_type() const { return m_mask; }

    private:
        /** Holds the bit mask. */
        base_type m_mask;
    };

    template<typename Enum>
    typename std::enable_if<EnableBitMaskOperators<Enum>::enable, EnumFlags<Enum>>::type operator|(Enum lhs, Enum rhs)
    {
        return EnumFlags<Enum>(lhs) | rhs;
    }

    template<typename Enum>
    typename std::enable_if<EnableBitMaskOperators<Enum>::enable, EnumFlags<Enum>>::type operator~(Enum rhs)
    {
        return ~(EnumFlags<Enum>(static_cast<typename EnumFlags<Enum>::base_type>(rhs)));
    }
}

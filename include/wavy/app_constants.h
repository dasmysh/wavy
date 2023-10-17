/**
 * @file   constants.h
 * @author Sebastian Maisch <Sebastian Maisch_EMAIL>
 * @date   2023.10.06
 *
 * @brief  Contains global constant definitions.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace fluid {
    /** The application name. */
    constexpr std::string_view applicationName = "wavy";
    /** The application version. */
    constexpr std::uint32_t applicationVersionMajor = 0;
    constexpr std::uint32_t applicationVersionMinor = 0;
    constexpr std::uint32_t applicationVersionPatch = 1;
    constexpr std::uint32_t applicationVersion = ((applicationVersionMajor << 22u) | (applicationVersionMinor << 12u) | applicationVersionPatch);

    /** The log file name. */
    constexpr std::string_view logFileName = "application.log";
    /** Use a timestamp for the log files. */
    constexpr bool LOG_USE_TIMESTAMPS = false;
    /** Log file application tag. */
    constexpr std::string_view logTag = "wavy";

#ifdef NDEBUG
    constexpr bool debug_build = false;
#else
    constexpr bool debug_build = true;
#endif

    enum class InterpolationMethod
    {
        Linear, Cubic
    };

    enum class IntegrationMethod
    {
        RK2, RK3, RK4
    };

    constexpr InterpolationMethod interpolation_method = InterpolationMethod::Linear;
    constexpr IntegrationMethod integration_method = IntegrationMethod::RK2;
}

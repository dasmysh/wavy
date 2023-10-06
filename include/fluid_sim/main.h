/**
 * @file   main.h
 * @author Sebastian Maisch <Sebastian Maisch_EMAIL>
 * @date   2023.10.06
 *
 * @brief  Includes common headers and defines common templates.
 */

#pragma once

#pragma warning(push, 3)
#include <array>
// ReSharper disable CppUnusedIncludeDirective
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <exception>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <span>
#pragma warning(pop)

#define GLM_FORCE_DEPTH_ZERO_TO_ONE


#include <spdlog/spdlog.h>

#include "app_constants.h"

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CONCAT(a, b) a ## b
#define CONCAT2(a, b) CONCAT(a, b)
#define UNIQUENAME(prefix) CONCAT2(prefix, __LINE__)
// NOLINTEND(cppcoreguidelines-macro-usage)

// ReSharper restore CppUnusedIncludeDirective

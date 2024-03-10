/**
 * @file   test_for_each_cs.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.09
 *
 * @brief  Tests for the for_each compute shader emulation.
 */

#include <catch.hpp>
#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

namespace wavy::utils
{
    TEST_CASE("wavy::utils::for_each_cs.for_each_cs", "")
    {
        spdlog::info("Starting parallel tasks with {} threads", std::thread::hardware_concurrency());
    }
}

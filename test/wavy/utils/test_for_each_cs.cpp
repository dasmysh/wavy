/**
 * @file   test_for_each_cs.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.09
 *
 * @brief  Tests for the for_each compute shader emulation.
 */

#include "utils/zip.h"

#include <catch.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/sync_wait.hpp>
#include <spdlog/spdlog.h>
#include <numeric>

namespace wavy::utils
{
    template<typename Pred> cppcoro::task<> run_on_thread_pool(cppcoro::static_thread_pool& tp, Pred work, int i)
    {
        co_await tp.schedule();

        [[maybe_unused]] auto result = co_await work(i);

    }

    template<typename Pred>
    cppcoro::task<> run_all_items(cppcoro::static_thread_pool& tp, const std::vector<int>& items, Pred work)
    {
        std::vector<cppcoro::task<>> awaitables(items.size());
        std::ranges::for_each(zip(items, awaitables), [&tp, &work](auto item_awaitable) {
            auto& item = std::get<0>(item_awaitable);
            auto& awaitable = std::get<1>(item_awaitable);
            awaitable = run_on_thread_pool(tp, work, item);
        });

        co_await cppcoro::when_all(std::move(awaitables));
    }

    TEST_CASE("wavy::utils::for_each_cs.for_each_cs", "")
    {
        spdlog::info("Starting parallel tasks with {} threads", std::thread::hardware_concurrency());

        cppcoro::static_thread_pool tp;

        std::vector<int> items(20);
        std::ranges::iota(items, 0);

        cppcoro::sync_wait(run_all_items(tp, items, [](int i) -> cppcoro::task<int> {
            spdlog::info("doing work: {}.", i);
            co_return 5;
        }));
    }
}

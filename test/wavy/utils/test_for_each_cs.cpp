/**
 * @file   test_for_each_cs.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.09
 *
 * @brief  Tests for the for_each compute shader emulation.
 */

#include "utils/compute_shader_cpu_emulation.h"
#include "utils/zip.h"

#include <catch.hpp>
#include <cppcoro/when_all.hpp>
#include <spdlog/spdlog.h>
#include <numeric>

namespace wavy::utils
{
    cppcoro::task<> run_on_thread_pool(cppcoro::static_thread_pool& tp, const cppcoro::task<>& kernel)
    {
        co_await tp.schedule();
        co_await kernel;
    }

    cppcoro::task<> resume_on_thread_pool(cppcoro::static_thread_pool& tp, const cppcoro::task<>& kernel)
    {
        co_await tp.schedule();
        kernel.when_ready().m_coroutine.resume();
        spdlog::error("resumed kernel done.");
    }

    template<typename Pred>
    cppcoro::task<> run_all_items(const std::shared_ptr<cppcoro::static_thread_pool>& tp,
        async_barrier_on_threadpool& barrier,
        const std::vector<int>& items, Pred work)
    {
        spdlog::info("tasks ({}) created, starting...", items.size());

        std::vector<cppcoro::task<>> awaitables(items.size());
        std::vector<cppcoro::task<>> work_awaitables(items.size());
        std::ranges::for_each(zip(items, awaitables, work_awaitables), [&tp, &work](auto item_awaitables) {
            auto& item = std::get<0>(item_awaitables);
            auto& awaitable = std::get<1>(item_awaitables);
            auto& work_awaitable = std::get<2>(item_awaitables);
            work_awaitable = work(item);
            awaitable = resume_on_thread_pool(*tp, work_awaitable);
        });

        std::size_t ready_counter = 0;
        std::size_t resume_counter = 0;
        std::vector<bool> ready_states(items.size(), false);
        while (ready_counter < items.size()) {
            for (const auto& awaitable : awaitables) { awaitable.when_ready().m_coroutine.resume(); }

            while (!barrier.is_ready() && ready_counter < items.size()) {
                for (std::size_t i = 0; i < ready_states.size(); ++i) {
                    if (work_awaitables[i].is_ready() && !ready_states[i]) {
                        ready_counter += 1;
                        ready_states[i] = true;
                    }
                }
                co_await barrier.scheduling();
            }

            if (ready_counter == items.size()) { continue; }

            barrier.reset();
            std::ranges::for_each(zip(awaitables, work_awaitables, ready_states),
                                  [&tp, &ready_counter](auto awaitable_items) {
                                      auto& awaitable = std::get<0>(awaitable_items);
                                      auto& work_awaitable = std::get<1>(awaitable_items);
                                      auto& ready_state = std::get<2>(awaitable_items);
                                      auto ready = work_awaitable.is_ready();
                                      if (!ready) {
                                          awaitable = resume_on_thread_pool(*tp, work_awaitable);
                                      } else if (!ready_state) {
                                          ready_counter += 1;
                                          ready_state = true;
                                      }
                                  });

            spdlog::error("resumed round {}", resume_counter);
            resume_counter += 1;
        }

        // for (const auto& awaitable : awaitables) { awaitable.when_ready().m_coroutine.resume(); }
        //
        // while (!barrier.is_ready()) { co_await barrier.scheduling(); }
        // awaitables[0].when_ready().m_coroutine.resume();

        // co_await cppcoro::when_all(std::move(awaitables));

        spdlog::error("tasks ({}) done?", items.size());
        co_return;
    }

    cppcoro::task<> work_wrapper(auto&& work, auto& barrier)
    {
        spdlog::info("task created, starting: ");
        work.when_ready().m_coroutine.resume();
        spdlog::info("barrier hit");

        barrier.count_down();
        spdlog::info("task finished?");
        co_return;
    }

    TEST_CASE("wavy::utils::for_each_cs.for_each_cs", "")
    {
        spdlog::info("Starting parallel tasks with {} threads", std::thread::hardware_concurrency());

        auto tp = std::make_shared<cppcoro::static_thread_pool>(2);

        constexpr std::size_t num_elements = 6;

        std::vector<int> items(num_elements);
        std::ranges::iota(items, 0);

        async_barrier_on_threadpool barrier{num_elements};

        auto work = [&barrier](int i) -> cppcoro::task<> {

            spdlog::info("doing work: {} 1/3.", i);

            co_await barrier;

            spdlog::info("doing work: {} 2/3.", i);

            co_await barrier;

            spdlog::info("doing work: {} 3/3.", i);
            co_return;
        };

        auto scheduler = run_all_items(tp, barrier, items, std::move(work));

        bool done = false;
        while (!done) {
            scheduler.when_ready().m_coroutine.resume();
            done = scheduler.when_ready().m_coroutine.done();
        }


        spdlog::error("scheduling finished.");
        // emulate_compute_shader(glm::uvec3{1}, glm::uvec3{1}, 0,
        //                        [](work_group_info, glm::uvec3, glm::uvec3, unsigned) -> cppcoro::task<> { co_return; });
    }
}

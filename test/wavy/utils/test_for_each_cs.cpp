/**
 * @file   test_for_each_cs.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.09
 *
 * @brief  Tests for the for_each compute shader emulation.
 */

#include <utils/compute_shader_cpu_emulation.h>
#include <utils/zip.h>

#include <catch.hpp>
#include <cppcoro/when_all.hpp>
#include <spdlog/spdlog.h>
#include <numeric>

namespace wavy::utils
{
    cppcoro::task<> resume_on_thread_pool_test(cppcoro::static_thread_pool& tp,
                                          std::shared_ptr<async_barrier> scheduling_finsied_barrier,
                                          const cppcoro::task<>& kernel)
    {
        auto coroutine = kernel.when_ready().m_coroutine;
        co_await tp.schedule();
        coroutine.resume();
        spdlog::error("resumed kernel done.");
        scheduling_finsied_barrier->count_down();
    }

    template<typename Pred>
    cppcoro::task<> run_all_items(std::shared_ptr<cppcoro::static_thread_pool> tp,
                                  std::shared_ptr<async_barrier> barrier,
                                  const std::vector<int>& items, Pred work)
    {
        spdlog::info("tasks ({}) created, starting...", items.size());

        std::vector<cppcoro::task<>> awaitables(items.size());
        std::vector<cppcoro::task<>> work_awaitables(items.size());
        auto scheduling_finished_barrier = std::make_shared<async_barrier>(static_cast<std::ptrdiff_t>(items.size()));

        std::ranges::for_each(
            zip(items, awaitables, work_awaitables), [&tp, &work, &scheduling_finished_barrier](auto item_awaitables) {
                auto& item = std::get<0>(item_awaitables);
                auto& awaitable = std::get<1>(item_awaitables);
                auto& work_awaitable = std::get<2>(item_awaitables);
                work_awaitable = work(item);
                awaitable = resume_on_thread_pool_test(*tp, scheduling_finished_barrier, work_awaitable);
            });

        std::size_t resume_counter = 0;
        bool all_kernels_done = false;
        while (!all_kernels_done) {
            for (const auto& awaitable : awaitables) { awaitable.when_ready().m_coroutine.resume(); }

            while (!scheduling_finished_barrier->is_ready()) { co_await scheduling_finished_barrier->scheduling(); }

            barrier->reset();
            scheduling_finished_barrier->reset();

            all_kernels_done = true;
            std::ranges::for_each(zip(awaitables, work_awaitables),
                                  [&tp, &all_kernels_done, &scheduling_finished_barrier](auto awaitable_items) {
                                      auto& awaitable = std::get<0>(awaitable_items);
                                      auto& work_awaitable = std::get<1>(awaitable_items);
                                      auto ready = work_awaitable.is_ready();
                                      if (!ready) {
                                          all_kernels_done = false;
                                          awaitable = resume_on_thread_pool_test(*tp, scheduling_finished_barrier,
                                                                                 work_awaitable);
                                      }
                                  });

            spdlog::error("resumed round {}", resume_counter);
            resume_counter += 1;
        }

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

        auto barrier = std::make_shared<async_barrier>(num_elements);

        auto work = [barrier](int i) -> cppcoro::task<> {
            spdlog::info("doing work: {} 1/3.", i);

            co_await *barrier;

            spdlog::info("doing work: {} 2/3.", i);

            co_await *barrier;

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
        emulate_compute_shader(glm::uvec3{1}, glm::uvec3{1}, 0,
                               [](work_group_info winfo, glm::uvec3 local_invocation_id,
                                  glm::uvec3 global_invocation_id, unsigned local_invocation_index) -> cppcoro::task<> {
                                   spdlog::info("doing cs work: ({}, {}, {})) / ({}, {}, {}) / {} 1/3.",
                                                local_invocation_id.x, local_invocation_id.y, local_invocation_id.z,
                                                global_invocation_id.x, global_invocation_id.y, global_invocation_id.z,
                                                local_invocation_index);

                                   co_await *winfo.barrier;

                                   spdlog::info("doing cs work: ({}, {}, {})) / ({}, {}, {}) / {} 2/3.",
                                                local_invocation_id.x, local_invocation_id.y, local_invocation_id.z,
                                                global_invocation_id.x, global_invocation_id.y, global_invocation_id.z,
                                                local_invocation_index);

                                   co_await *winfo.barrier;

                                   spdlog::info("doing cs work: ({}, {}, {})) / ({}, {}, {}) / {} 3/3.",
                                                local_invocation_id.x, local_invocation_id.y, local_invocation_id.z,
                                                global_invocation_id.x, global_invocation_id.y, global_invocation_id.z,
                                                local_invocation_index);
                                   co_return;
                               });
    }
}

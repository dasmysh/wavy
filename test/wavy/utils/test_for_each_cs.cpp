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

#define THREADSAVE_CHECK_EQ(atomic_var, a, b) \
{ \
    bool expected = true; \
    atomic_var.compare_exchange_strong(expected, a == b); \
    if (a != b) { \
        spdlog::error(#a "({}) != " #b "({})", a, b); \
    } \
}

namespace wavy::utils
{
    namespace detail
    {
        struct cs_kernel_local_info
        {
            cs_kernel_local_info(const work_group_info& winfo, const glm::uvec3& global_invocation_id,
                                 const glm::uvec3& local_invocation_id)
                : work_group_index{winfo.work_group_id.z * winfo.num_work_groups.y * winfo.num_work_groups.x
                                   + winfo.work_group_id.y * winfo.num_work_groups.x + winfo.work_group_id.x}
                , work_group_size_linear{winfo.work_group_size.x * winfo.work_group_size.y * winfo.work_group_size.z}
                , global_size{winfo.num_work_groups * winfo.work_group_size}
                , global_work_group_start_offset{winfo.work_group_id * winfo.work_group_size}
                , global_invocation_index{global_invocation_id.z * global_size.y * global_size.x
                                          + global_invocation_id.y * global_size.x + global_invocation_id.x}
                , global_work_group_start_index{global_work_group_start_offset.z * global_size.y * global_size.x
                                          + global_work_group_start_offset.y * global_size.x
                                          + global_work_group_start_offset.x}
                , local_index_as_global_offset{local_invocation_id.z * global_size.y * global_size.x
                                               + local_invocation_id.y * global_size.x + local_invocation_id.x}
            {}

            std::size_t work_group_index = 0;
            std::size_t work_group_size_linear = 0;
            glm::uvec3 global_size;
            glm::uvec3 global_work_group_start_offset;
            std::size_t global_invocation_index = 0;
            std::size_t global_work_group_start_index = 0;
            std::size_t local_index_as_global_offset = 0;
        };
    }

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

    TEST_CASE("wavy::utils::emulate_compute_shader.simple execution", "")
    {
        bool single_thread_executed = false;
        auto kernel = [&single_thread_executed](work_group_info winfo, glm::uvec3 local_invocation_id,
                                                glm::uvec3 global_invocation_id,
                                                unsigned local_invocation_index) -> cppcoro::task<> {
            single_thread_executed = true;
            co_return;
        };

        emulate_compute_shader(glm::uvec3{1}, glm::uvec3{1}, 0, kernel);

        CHECK(single_thread_executed == true);
    }

    TEST_CASE("wavy::utils::emulate_compute_shader.multiple threads in one workgroup 1D", "")
    {
        std::vector<std::uint8_t> thread_executed(100, std::uint8_t(0));
        auto kernel = [&thread_executed](work_group_info winfo, glm::uvec3 local_invocation_id,
                                                glm::uvec3 global_invocation_id,
                                                unsigned local_invocation_index) -> cppcoro::task<> {
            thread_executed[local_invocation_index] = 1;
            co_return;
        };

        emulate_compute_shader(glm::uvec3{1}, glm::uvec3{100, 1, 1}, 0, kernel);

        for (const auto& executed : thread_executed) { CHECK(executed == 1); }
    }

    TEST_CASE("wavy::utils::emulate_compute_shader.multiple threads in one workgroup 2D", "")
    {
        std::vector<std::uint8_t> thread_executed_linear(100, std::uint8_t(0));
        std::mdspan<std::uint8_t, std::dextents<std::size_t, 2>> thread_executed(thread_executed_linear.data(), 10, 10);

        auto kernel = [&thread_executed](work_group_info winfo, glm::uvec3 local_invocation_id,
                                         glm::uvec3 global_invocation_id,
                                         unsigned local_invocation_index) -> cppcoro::task<> {
            thread_executed[std::array<std::size_t, 2>{global_invocation_id.x, global_invocation_id.y}] = 1;
            co_return;
        };

        emulate_compute_shader(glm::uvec3{1}, glm::uvec3{10, 10, 1}, 0, kernel);

        for (const auto& executed : thread_executed_linear) { CHECK(executed == 1); }
    }

    TEST_CASE("wavy::utils::emulate_compute_shader.multiple threads in one workgroup 3D", "")
    {
        std::vector<std::uint8_t> thread_executed_linear(125,
                                                         std::uint8_t(0));
        std::mdspan<std::uint8_t, std::dextents<std::size_t, 3>> thread_executed(thread_executed_linear.data(), 5, 5,
                                                                                 5);

        auto kernel = [&thread_executed](work_group_info winfo, glm::uvec3 local_invocation_id,
                                         glm::uvec3 global_invocation_id,
                                         unsigned local_invocation_index) -> cppcoro::task<> {
            thread_executed[std::array<std::size_t, 3>{global_invocation_id.x, global_invocation_id.y,
                                                       global_invocation_id.z}] = 1;
            co_return;
        };

        emulate_compute_shader(glm::uvec3{1}, glm::uvec3{5, 5, 5}, 0, kernel);

        for (const auto& executed : thread_executed_linear) { CHECK(executed == 1); }
    }

    TEST_CASE("wavy::utils::emulate_compute_shader.multiple threads in multiple workgroups 1D/1D", "")
    {
        constexpr glm::uvec3 work_groups{100, 1, 1};
        constexpr std::size_t work_groups_linear = work_groups.x * work_groups.y * work_groups.z;
        constexpr glm::uvec3 work_group_size{100, 1, 1};
        constexpr std::size_t work_group_size_linear = work_group_size.x * work_group_size.y * work_group_size.z;

        std::vector<std::uint8_t> thread_executed(work_groups_linear * work_group_size_linear, 0);
        std::vector<std::atomic_size_t> local_thread_counts(work_group_size_linear);
        for (auto& local_thread_count : local_thread_counts) { local_thread_count.store(0); }

        std::atomic_bool atomic_all_work_groups_indices_correct = true;
        std::atomic_bool atomic_all_invocation_indices_correct_1d = true;
        std::atomic_bool atomic_all_invocation_indices_correct = true;

        auto kernel = [&thread_executed, &local_thread_counts, &atomic_all_work_groups_indices_correct,
                       &atomic_all_invocation_indices_correct_1d, &atomic_all_invocation_indices_correct](
                          work_group_info winfo, glm::uvec3 local_invocation_id, glm::uvec3 global_invocation_id,
                          unsigned local_invocation_index) -> cppcoro::task<> {
            detail::cs_kernel_local_info local_info{winfo, global_invocation_id, local_invocation_id};

            THREADSAVE_CHECK_EQ(atomic_all_work_groups_indices_correct, local_info.work_group_index,
                                winfo.work_group_id.x);

            THREADSAVE_CHECK_EQ(atomic_all_invocation_indices_correct_1d, local_info.global_invocation_index,
                                global_invocation_id.x);

            THREADSAVE_CHECK_EQ(atomic_all_invocation_indices_correct, local_info.global_invocation_index,
                                local_info.global_work_group_start_index + local_invocation_index);

            thread_executed[local_info.global_invocation_index] = 1;
            local_thread_counts[local_invocation_index] += 1;
            co_return;
        };

        emulate_compute_shader(work_groups, work_group_size, 0, kernel);

        bool all_work_groups_indices_correct = atomic_all_work_groups_indices_correct.load();
        bool all_invocation_indices_correct_1d = atomic_all_invocation_indices_correct_1d.load();
        bool all_invocation_indices_correct = atomic_all_invocation_indices_correct.load();
        CHECK(all_work_groups_indices_correct == true);
        CHECK(all_invocation_indices_correct_1d == true);
        CHECK(all_invocation_indices_correct == true);

        for (const auto& executed : thread_executed) { CHECK(executed == 1); }
        for (const auto& local_thread_count : local_thread_counts) {
            auto work_group_count = local_thread_count.load();
            CHECK(work_group_count == work_groups_linear);
        }
    }

    TEST_CASE("wavy::utils::emulate_compute_shader.multiple threads in multiple workgroups 2D/2D", "")
    {
        constexpr glm::uvec3 work_groups{10, 10, 1};
        constexpr std::size_t work_groups_linear = work_groups.x * work_groups.y * work_groups.z;
        constexpr glm::uvec3 work_group_size{10, 10, 1};
        constexpr std::size_t work_group_size_linear = work_group_size.x * work_group_size.y * work_group_size.z;

        std::vector<std::uint8_t> thread_executed_linear(work_groups_linear * work_group_size_linear, 0);
        std::mdspan<std::uint8_t, std::dextents<std::size_t, 2>> thread_executed(
            thread_executed_linear.data(), work_groups.x * work_group_size.x, work_groups.y * work_group_size.y);

        std::vector<std::atomic_size_t> local_thread_count_linear(work_group_size_linear);
        for (auto& local_thread_count : local_thread_count_linear) { local_thread_count.store(0); }
        std::mdspan<std::atomic_size_t, std::dextents<std::size_t, 2>> local_thread_count(
            local_thread_count_linear.data(), work_group_size.x, work_group_size.y);

        std::atomic_bool atomic_all_invocation_indices_correct = true;

        auto kernel = [&thread_executed, &local_thread_count, &atomic_all_invocation_indices_correct](
                          work_group_info winfo, glm::uvec3 local_invocation_id,
                                         glm::uvec3 global_invocation_id,
                                         unsigned local_invocation_index) -> cppcoro::task<> {
            detail::cs_kernel_local_info local_info{winfo, global_invocation_id, local_invocation_id};

            THREADSAVE_CHECK_EQ(atomic_all_invocation_indices_correct, local_info.global_invocation_index,
                                local_info.global_work_group_start_index + local_info.local_index_as_global_offset);


            thread_executed[std::array<std::size_t, 2>{global_invocation_id.x, global_invocation_id.y}] = 1;
            local_thread_count[std::array<std::size_t, 2>{local_invocation_id.x, local_invocation_id.y}] += 1;
            co_return;
        };

        emulate_compute_shader(work_groups, work_group_size, 0, kernel);

        bool all_invocation_indices_correct = atomic_all_invocation_indices_correct.load();
        CHECK(all_invocation_indices_correct == true);

        for (const auto& executed : thread_executed_linear) { CHECK(executed == 1); }
        for (const auto& work_group_count_atomic : local_thread_count_linear) {
            auto work_group_count = work_group_count_atomic.load();
            CHECK(work_group_count == work_groups_linear);
        }
    }

    TEST_CASE("wavy::utils::emulate_compute_shader.multiple threads in multiple workgroups 3D/3D", "")
    {
        constexpr glm::uvec3 work_groups{5, 5, 5};
        constexpr std::size_t work_groups_linear = work_groups.x * work_groups.y * work_groups.z;
        constexpr glm::uvec3 work_group_size{5, 5, 5};
        constexpr std::size_t work_group_size_linear = work_group_size.x * work_group_size.y * work_group_size.z;

        std::vector<std::uint8_t> thread_executed_linear(work_groups_linear * work_group_size_linear, 0);
        std::mdspan<std::uint8_t, std::dextents<std::size_t, 3>> thread_executed(
            thread_executed_linear.data(), work_groups.x * work_group_size.x, work_groups.y * work_group_size.y,
            work_groups.z * work_group_size.z);

        std::vector<std::atomic_size_t> local_thread_count_linear(work_group_size_linear);
        for (auto& local_thread_count : local_thread_count_linear) { local_thread_count.store(0); }
        std::mdspan<std::atomic_size_t, std::dextents<std::size_t, 3>> local_thread_count(
            local_thread_count_linear.data(), work_group_size.x, work_group_size.y, work_group_size.z);

        std::array<std::atomic_bool, 2> atomic_all_invocation_indices_correct{true, true};

        auto kernel = [&thread_executed, &atomic_all_invocation_indices_correct](
                          work_group_info winfo, glm::uvec3 local_invocation_id,
                                         glm::uvec3 global_invocation_id,
                          unsigned local_invocation_index) -> cppcoro::task<> {
            detail::cs_kernel_local_info local_info{winfo, global_invocation_id, local_invocation_id};

            THREADSAVE_CHECK_EQ(atomic_all_invocation_indices_correct[0], local_info.global_invocation_index,
                                local_info.global_work_group_start_index + local_invocation_index);

            THREADSAVE_CHECK_EQ(atomic_all_invocation_indices_correct[1], local_info.global_invocation_index,
                                local_info.work_group_index * local_info.work_group_size_linear
                                    + local_invocation_index);


            thread_executed[std::array<std::size_t, 3>{global_invocation_id.x, global_invocation_id.y,
                                                       global_invocation_id.z}] = 1;
            co_return;
        };

        emulate_compute_shader(work_groups, work_group_size, 0, kernel);

        bool all_invocation_indices_correct_0 = atomic_all_invocation_indices_correct[0].load();
        bool all_invocation_indices_correct_1 = atomic_all_invocation_indices_correct[1].load();
        CHECK(all_invocation_indices_correct_0 == true);
        CHECK(all_invocation_indices_correct_1 == true);

        for (const auto& executed : thread_executed_linear) { CHECK(executed == 1); }
    }


    TEST_CASE("wavy::utils::emulate_compute_shader.playground", "")
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

/**
 * @file   compute_shader_cpu_emulation.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.13
 *
 * @brief  Compute shader emulation on the CPU.
 */

#pragma once

#include "utils/async_barrier.h"

#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>
#include <glm/vec3.hpp>
#pragma warning(push)
#pragma warning(disable : 5246)
#include <mdspan>
#pragma warning(pop)

namespace wavy::utils {

    using barrier_span_type = std::mdspan<std::shared_ptr<async_barrier>, std::dextents<std::size_t, 3>>;
    using shared_memory_span_type = std::mdspan<std::uint8_t, std::dextents<std::size_t, 4>>;
    using task_span_type = std::mdspan<cppcoro::task<>, std::dextents<std::size_t, 3>>;

    struct work_group_info
    {
        glm::uvec3 num_work_groups;
        glm::uvec3 work_group_size;
        glm::uvec3 work_group_id;
        std::shared_ptr<async_barrier> barrier;
        std::span<std::uint8_t> shared_memory;
    };

    cppcoro::task<> resume_on_thread_pool(cppcoro::static_thread_pool& tp,
                                          std::shared_ptr<async_barrier> scheduling_finsied_barrier,
                                          const cppcoro::task<>& kernel);

    template<typename Pred>
    cppcoro::task<>
    schedule_emulated_compute_shader_work_group(std::shared_ptr<cppcoro::static_thread_pool> worker_thread_pool,
                                                work_group_info winfo, Pred kernel)
    {
        std::size_t work_group_size_linear =
            winfo.work_group_size.x * winfo.work_group_size.y * winfo.work_group_size.z;
        std::vector<cppcoro::task<>> awaitables_linear(work_group_size_linear);
        std::vector<cppcoro::task<>> kernel_awaitables_linear(work_group_size_linear);
        auto scheduling_finished_barrier =
            std::make_shared<async_barrier>(static_cast<std::ptrdiff_t>(work_group_size_linear));

        task_span_type awaitables(awaitables_linear.data(), winfo.work_group_size.x, winfo.work_group_size.y,
                                  winfo.work_group_size.z);
        task_span_type kernel_awaitables(kernel_awaitables_linear.data(), winfo.work_group_size.x,
                                         winfo.work_group_size.y, winfo.work_group_size.z);

        auto barrier = winfo.barrier;

        for (std::size_t liz = 0; liz < winfo.work_group_size.z; ++liz) {
            for (std::size_t liy = 0; liy < winfo.work_group_size.y; ++liy) {
                for (std::size_t lix = 0; lix < winfo.work_group_size.x; ++lix) {
                    glm::uvec3 local_invocation_id{lix, liy, liz};
                    glm::uvec3 global_invocation_id = winfo.work_group_id * winfo.work_group_size + local_invocation_id;
                    unsigned int local_invocation_index =
                        local_invocation_id.z * winfo.work_group_size.x * winfo.work_group_size.y
                        + local_invocation_id.y * winfo.work_group_size.x + local_invocation_id.x;

                    auto& awaitable = awaitables[std::array<std::size_t, 3>{{lix, liy, liz}}];
                    auto& kernel_awaitable = kernel_awaitables[std::array<std::size_t, 3>{{lix, liy, liz}}];
                    kernel_awaitable = kernel(winfo, local_invocation_id, global_invocation_id, local_invocation_index);
                    awaitable =
                        resume_on_thread_pool(*worker_thread_pool, scheduling_finished_barrier, kernel_awaitable);
                }
            }
        }

        bool all_kernels_done = false;
        while (!all_kernels_done) {
            for (const auto& awaitable : awaitables_linear) { awaitable.when_ready().m_coroutine.resume(); }

            while (!scheduling_finished_barrier->is_ready()) { co_await scheduling_finished_barrier->scheduling(); }

            barrier->reset();
            scheduling_finished_barrier->reset();

            all_kernels_done = true;
            for (std::size_t i = 0; i < awaitables_linear.size(); ++i) {
                auto& awaitable = awaitables_linear[i];
                auto& kernel_awaitable = kernel_awaitables_linear[i];
                if (!kernel_awaitable.is_ready()) {
                    all_kernels_done = false;
                    awaitable =
                        resume_on_thread_pool(*worker_thread_pool, scheduling_finished_barrier, kernel_awaitable);
                }
            }
        }
    }

    template<typename Pred>
    cppcoro::task<> emulate_compute_shader_schedule_work_groups(const glm::uvec3& work_groups,
                                                                const glm::uvec3& work_group_size,
                                                                std::size_t shared_memory_size, Pred kernel)
    {
        std::size_t work_groups_linear = work_groups.x * work_groups.y * work_groups.z;
        std::size_t work_group_size_linear = work_group_size.x * work_group_size.y * work_group_size.z;

        auto work_groups_thread_pool =
            std::make_shared<cppcoro::static_thread_pool>(static_cast<std::uint32_t>(work_groups_linear));
        auto worker_thread_pool = std::make_shared<cppcoro::static_thread_pool>();

        std::vector<std::shared_ptr<async_barrier>> barriers_linear(work_groups_linear);
        std::vector<std::uint8_t> shared_memory_linear(work_groups_linear * shared_memory_size);
        std::vector<cppcoro::task<>> awaitables_linear(work_groups_linear);
        std::vector<cppcoro::task<>> work_group_awaitables_linear(work_groups_linear);

        auto scheduling_finished_barrier =
            std::make_shared<async_barrier>(static_cast<std::ptrdiff_t>(work_group_size_linear));

        barrier_span_type barriers(barriers_linear.data(), work_groups.x, work_groups.y, work_groups.z);
        shared_memory_span_type shared_memory(shared_memory_linear.data(), shared_memory_size, work_groups.x,
                                              work_groups.y, work_groups.z);
        task_span_type awaitables(awaitables_linear.data(), work_groups.x, work_groups.y, work_groups.z);
        task_span_type work_group_awaitables(work_group_awaitables_linear.data(), work_groups.x, work_groups.y,
                                             work_groups.z);

        for (std::size_t wiz = 0; wiz < work_groups.z; ++wiz) {
            for (std::size_t wiy = 0; wiy < work_groups.y; ++wiy) {
                for (std::size_t wix = 0; wix < work_groups.x; ++wix) {
                    glm::uvec3 work_group_id{wix, wiy, wiz};

                    auto& barrier =
                        barriers[std::array<std::size_t, 3>{{work_group_id.x, work_group_id.y, work_group_id.z}}];
                    barrier = std::make_shared<async_barrier>(static_cast<std::ptrdiff_t>(work_group_size_linear));
                    work_group_info winfo{
                        .num_work_groups = work_groups,
                        .work_group_size = work_group_size,
                        .work_group_id = work_group_id,
                        .barrier = barrier,
                        .shared_memory{shared_memory_size > 0 ? &shared_memory[std::array<std::size_t, 4>{
                                           {0, work_group_id.x, work_group_id.y, work_group_id.z}}]
                                                              : nullptr,
                                       shared_memory_size}};

                    auto& awaitable = awaitables[std::array<std::size_t, 3>{{wix, wiy, wiz}}];
                    auto& work_group_awaitable = work_group_awaitables[std::array<std::size_t, 3>{{wix, wiy, wiz}}];

                    work_group_awaitable =
                        schedule_emulated_compute_shader_work_group(worker_thread_pool, winfo, kernel);
                    awaitable = resume_on_thread_pool(*work_groups_thread_pool, scheduling_finished_barrier,
                                                      work_group_awaitable);
                }
            }
        }

        bool all_work_groups_done = false;
        while (!all_work_groups_done) {
            for (const auto& awaitable : awaitables_linear) { awaitable.when_ready().m_coroutine.resume(); }

            while (!scheduling_finished_barrier->is_ready()) { co_await scheduling_finished_barrier->scheduling(); }

            scheduling_finished_barrier->reset();

            all_work_groups_done = true;
            for (std::size_t i = 0; i < awaitables_linear.size(); ++i) {
                auto& awaitable = awaitables_linear[i];
                auto& work_group_awaitable = work_group_awaitables_linear[i];
                if (!work_group_awaitable.is_ready()) {
                    all_work_groups_done = false;
                    awaitable = resume_on_thread_pool(*work_groups_thread_pool, scheduling_finished_barrier,
                                                      work_group_awaitable);
                }
            }
        }
    }

    template<typename Pred>
    void emulate_compute_shader(const glm::uvec3& work_groups, const glm::uvec3& work_group_size,
                                std::size_t shared_memory_size, Pred kernel)
    {
        auto scheduler =
            emulate_compute_shader_schedule_work_groups(work_groups, work_group_size, shared_memory_size, kernel);

        bool done = false;
        while (!done) {
            scheduler.when_ready().m_coroutine.resume();
            done = scheduler.when_ready().m_coroutine.done();
        }
    }
}

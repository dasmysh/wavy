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

    using barrier_span_type = std::mdspan<async_barrier, std::dextents<std::size_t, 3>>;
    using shared_memory_span_type = std::mdspan<std::uint8_t, std::dextents<std::size_t, 4>>;
    using task_span_type = std::mdspan<cppcoro::task<>, std::dextents<std::size_t, 3>>;

    struct work_group_info
    {
        glm::uvec3 num_work_groups;
        glm::uvec3 work_group_size;
        glm::uvec3 work_group_id;
        async_barrier& barrier;
        std::span<std::uint8_t> shared_memory;
    };

    cppcoro::task<> run_emulated_cs_kernel_on_thread_pool(cppcoro::static_thread_pool& tp, cppcoro::task<> kernel);
    cppcoro::task<> wait_for_emulated_cs_tasks(std::vector<cppcoro::task<>>&& awaitables);

    template<typename Pred>
    cppcoro::task<> schedule_emulated_compute_shader_work_group(cppcoro::static_thread_pool& tp, work_group_info winfo,
                                                                Pred kernel)
    {
        co_await tp.schedule();

        auto thread_pool = std::make_shared<cppcoro::static_thread_pool>();

        std::size_t work_group_size_linear =
            winfo.work_group_size.x * winfo.work_group_size.y * winfo.work_group_size.z;
        std::vector<cppcoro::task<>> awaitables_linear(work_group_size_linear);
        task_span_type awaitables(awaitables_linear.data(), winfo.work_group_size.x, winfo.work_group_size.y,
                                  winfo.work_group_size.z);

        async_barrier& barrier = winfo.barrier;

        // TODO:
        for (std::size_t liz = 0; liz < winfo.work_group_size.z; ++liz) {
            for (std::size_t liy = 0; liy < winfo.work_group_size.y; ++liy) {
                for (std::size_t lix = 0; lix < winfo.work_group_size.x; ++lix) {
                    glm::uvec3 local_invocation_id{lix, liy, liz};
                    glm::uvec3 global_invocation_id = winfo.work_group_id * winfo.work_group_size + local_invocation_id;
                    unsigned int local_invocation_index =
                        local_invocation_id.z * winfo.work_group_size.x * winfo.work_group_size.y
                        + local_invocation_id.y * winfo.work_group_size.x + local_invocation_id.x;

                    auto& awaitable = awaitables[std::array<std::size_t, 3>{{lix, liy, liz}}];

                    awaitable = run_emulated_cs_kernel_on_thread_pool(
                        *thread_pool, kernel(winfo, local_invocation_id, global_invocation_id, local_invocation_index));
                }
            }
        }

        co_await barrier;
         //.scheduling;
    }

    template<typename Pred>
    void emulate_compute_shader(const glm::uvec3& work_groups, const glm::uvec3& work_group_size,
                                std::size_t shared_memory_size, Pred kernel)
    {
        auto thread_pool = std::make_shared<cppcoro::static_thread_pool>();

        std::size_t work_groups_linear = work_groups.x * work_groups.y * work_groups.z;
        std::size_t work_group_size_linear = work_group_size.x * work_group_size.y * work_group_size.z;

        std::vector<async_barrier> barriers_linear(work_groups_linear);
        std::vector<std::uint8_t> shared_memory_linear(work_groups_linear * shared_memory_size);
        std::vector<cppcoro::task<>> awaitables_linear(work_groups_linear);

        barrier_span_type barriers(barriers_linear.data(), work_groups.x, work_groups.y, work_groups.z);
        shared_memory_span_type shared_memory(shared_memory_linear.data(), shared_memory_size, work_groups.x,
                                              work_groups.y, work_groups.z);
        task_span_type awaitables(awaitables_linear.data(), work_groups.x, work_groups.y, work_groups.z);

        for (std::size_t wiz = 0; wiz < work_groups.z; ++wiz) {
            for (std::size_t wiy = 0; wiy < work_groups.y; ++wiy) {
                for (std::size_t wix = 0; wix < work_groups.x; ++wix) {
                    glm::uvec3 work_group_id{wix, wiy, wiz};

                    auto& barrier =
                        barriers[std::array<std::size_t, 3>{{work_group_id.x, work_group_id.y, work_group_id.z}}];
                    barrier.reset(thread_pool, static_cast<std::ptrdiff_t>(work_group_size_linear));
                    work_group_info winfo{.num_work_groups = work_groups,
                                          .work_group_size = work_group_size,
                                          .work_group_id = work_group_id,
                                          .barrier = barrier,
                                          .shared_memory{&shared_memory[std::array<std::size_t, 4>{
                                                             {0, work_group_id.x, work_group_id.y, work_group_id.z}}],
                                                         shared_memory_size}};

                    awaitables[std::array<std::size_t, 3>{{wix, wiy, wiz}}] =
                        schedule_emulated_compute_shader_work_group(*thread_pool, winfo, kernel);
                }
            }
        }

        // cppcoro::sync_wait(wait_for_emulated_cs_tasks(std::move(awaitables_linear)));
        cppcoro::sync_wait(cppcoro::when_all(std::move(awaitables_linear)));
    }
}

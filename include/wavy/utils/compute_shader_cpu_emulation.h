/**
 * @file   compute_shader_cpu_emulation.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2024.03.13
 *
 * @brief  Compute shader emulation on the CPU.
 */

#pragma once

#include "utils/async_barrier.h"
#include "utils/synced_task.h"
#include "utils/task.h"

#include <coro/thread_pool.hpp>
#include <glm/vec3.hpp>
#include <mdspan>

namespace wavy::utils {
    template<class SharedMemoryType = void> struct work_group_info
    {
        glm::uvec3 num_work_groups;
        glm::uvec3 work_group_size;
        glm::uvec3 work_group_id;
        SharedMemoryType* shared_memory;
    };

    template<class SharedMemoryType> struct shared_memory_container
    {
        shared_memory_container(const glm::uvec3& work_groups, std::size_t work_groups_linear)
            : shared_memory_linear(work_groups_linear)
            , shared_memory(shared_memory_linear.data(), work_groups.x, work_groups.y, work_groups.z)
        {
        }

        SharedMemoryType* operator[](const glm::uvec3& work_group_id)
        {
            return &shared_memory[std::array<std::size_t, 3>{work_group_id.x, work_group_id.y, work_group_id.z}];
        }

        std::vector<SharedMemoryType> shared_memory_linear;
        std::mdspan<SharedMemoryType, std::dextents<std::size_t, 3>> shared_memory;
    };

    template<> struct shared_memory_container<void>
    {
        shared_memory_container(const glm::uvec3&, std::size_t) {}
        nullptr_t operator[](const glm::uvec3&) const { return nullptr; }
    };

    class compute_shader_emulator
    {
    public:
        explicit compute_shader_emulator(const glm::uvec3& work_groups)
            : m_work_groups{work_groups}
            , m_work_groups_linear{m_work_groups.x * m_work_groups.y * m_work_groups.z}
            , m_awaitables_linear{m_work_groups_linear}
            , m_work_group_awaitables_linear{m_work_groups_linear}
        {
        }

        template<class SharedMemoryType = void, typename Pred>
        void emulate_compute_shader(const glm::uvec3& work_group_size, Pred kernel);

        const glm::uvec3& get_work_groups() const { return m_work_groups; }

    private:
        template<class SharedMemoryType, typename Pred>
        task<> emulate_compute_shader_schedule_work_groups(const glm::uvec3& work_group_size, Pred kernel);
        template<class SharedMemoryType, typename Pred>
        task<> schedule_emulated_compute_shader_work_group(work_group_info<SharedMemoryType> winfo, Pred kernel);

        synced_task<> resume_on_worker_thread_pool(task<>& kernel);
        synced_task<> resume_on_work_groups_thread_pool(task<>& kernel);
        task<> resume_on_thread_pool(coro::thread_pool& tp, task<>& kernel) const;

        glm::uvec3 m_work_groups;
        std::size_t m_work_groups_linear;
        coro::thread_pool m_work_groups_thread_pool;
        coro::thread_pool m_worker_thread_pool;

        std::vector<synced_task<>> m_awaitables_linear;
        std::vector<task<>> m_work_group_awaitables_linear;
    };

    template<class SharedMemoryType, typename Pred>
    void compute_shader_emulator::emulate_compute_shader(const glm::uvec3& work_group_size, Pred kernel)
    {
        auto scheduler = emulate_compute_shader_schedule_work_groups<SharedMemoryType>(work_group_size, kernel);

        bool done = false;
        while (!done) {
            scheduler.resume();
            done = scheduler.is_ready();
        }
    }

    template<class SharedMemoryType, typename Pred>
    task<> compute_shader_emulator::emulate_compute_shader_schedule_work_groups(const glm::uvec3& work_group_size,
                                                                                      Pred kernel)
    {
        shared_memory_container<SharedMemoryType> shared_memory(m_work_groups, m_work_groups_linear);
        auto scheduling_finished_barrier =
            std::make_shared<async_barrier>(static_cast<std::ptrdiff_t>(m_work_groups_linear));

        std::mdspan awaitables(m_awaitables_linear.data(), m_work_groups.x, m_work_groups.y, m_work_groups.z);
        std::mdspan work_group_awaitables(m_work_group_awaitables_linear.data(), m_work_groups.x, m_work_groups.y,
                                          m_work_groups.z);

        for (std::size_t wiz = 0; wiz < m_work_groups.z; ++wiz) {
            for (std::size_t wiy = 0; wiy < m_work_groups.y; ++wiy) {
                for (std::size_t wix = 0; wix < m_work_groups.x; ++wix) {
                    glm::uvec3 work_group_id{wix, wiy, wiz};
                    work_group_info<SharedMemoryType> winfo{.num_work_groups = m_work_groups,
                                                            .work_group_size = work_group_size,
                                                            .work_group_id = work_group_id,
                                                            .shared_memory = shared_memory[work_group_id]};

                    auto& awaitable = awaitables[std::array<std::size_t, 3>{{wix, wiy, wiz}}];
                    auto& work_group_awaitable = work_group_awaitables[std::array<std::size_t, 3>{wix, wiy, wiz}];

                    work_group_awaitable = schedule_emulated_compute_shader_work_group(winfo, kernel);
                    awaitable = resume_on_work_groups_thread_pool(work_group_awaitable);
                }
            }
        }

        bool all_work_groups_done = false;
        while (!all_work_groups_done) {
            for (auto& awaitable : m_awaitables_linear) {
                if (!awaitable.is_ready()) { awaitable.start(*scheduling_finished_barrier); }
            }

            while (!scheduling_finished_barrier->is_ready()) { co_await scheduling_finished_barrier->scheduling(); }

            all_work_groups_done = true;
            std::ptrdiff_t unfinished_work_groups = 0;
            for (std::size_t i = 0; i < m_awaitables_linear.size(); ++i) {
                auto& awaitable = m_awaitables_linear[i];
                auto& work_group_awaitable = m_work_group_awaitables_linear[i];
                if (!work_group_awaitable.is_ready()) {
                    all_work_groups_done = false;
                    awaitable = resume_on_work_groups_thread_pool(work_group_awaitable);
                    unfinished_work_groups += 1;
                }
            }

            scheduling_finished_barrier->reset(unfinished_work_groups);
        }

        for (auto& awaitable : m_awaitables_linear)
        {
            awaitable = synced_task{};
        }
    }

    template<class SharedMemoryType, typename Pred>
    task<> compute_shader_emulator::schedule_emulated_compute_shader_work_group(work_group_info<SharedMemoryType> winfo,
                                                                                Pred kernel)
    {
        std::size_t work_group_size_linear =
            winfo.work_group_size.x * winfo.work_group_size.y * winfo.work_group_size.z;
        std::vector<synced_task<>> awaitables_linear(work_group_size_linear);
        std::vector<task<>> kernel_awaitables_linear(work_group_size_linear);
        auto scheduling_finished_barrier =
            std::make_shared<async_barrier>(static_cast<std::ptrdiff_t>(work_group_size_linear));

        std::mdspan awaitables(awaitables_linear.data(), winfo.work_group_size.x, winfo.work_group_size.y,
                               winfo.work_group_size.z);
        std::mdspan kernel_awaitables(kernel_awaitables_linear.data(), winfo.work_group_size.x, winfo.work_group_size.y,
                                      winfo.work_group_size.z);

        for (std::size_t liz = 0; liz < winfo.work_group_size.z; ++liz) {
            for (std::size_t liy = 0; liy < winfo.work_group_size.y; ++liy) {
                for (std::size_t lix = 0; lix < winfo.work_group_size.x; ++lix) {
                    glm::uvec3 local_invocation_id{lix, liy, liz};
                    glm::uvec3 global_invocation_id = winfo.work_group_id * winfo.work_group_size + local_invocation_id;
                    unsigned int local_invocation_index =
                        local_invocation_id.z * winfo.work_group_size.x * winfo.work_group_size.y
                        + local_invocation_id.y * winfo.work_group_size.x + local_invocation_id.x;

                    auto& awaitable = awaitables[std::array<std::size_t, 3>{lix, liy, liz}];
                    auto& kernel_awaitable = kernel_awaitables[std::array<std::size_t, 3>{lix, liy, liz}];
                    kernel_awaitable = kernel(winfo, local_invocation_id, global_invocation_id, local_invocation_index);
                    awaitable = resume_on_worker_thread_pool(kernel_awaitable);
                }
            }
        }

        bool all_kernels_done = false;
        while (!all_kernels_done) {
            for (auto& awaitable : awaitables_linear) {
                if (!awaitable.is_ready()) { awaitable.start(*scheduling_finished_barrier); }
            }

            while (!scheduling_finished_barrier->is_ready()) { co_await scheduling_finished_barrier->scheduling(); }

            all_kernels_done = true;
            std::ptrdiff_t unfinished_work_groups = 0;
            for (std::size_t i = 0; i < awaitables_linear.size(); ++i) {
                auto& awaitable = awaitables_linear[i];
                auto& kernel_awaitable = kernel_awaitables_linear[i];
                if (!kernel_awaitable.is_ready()) {
                    all_kernels_done = false;
                    awaitable = resume_on_worker_thread_pool(kernel_awaitable);
                    unfinished_work_groups += 1;
                }
            }

            scheduling_finished_barrier->reset(unfinished_work_groups);
        }

        for (auto& awaitable : awaitables_linear)
        {
            awaitable = synced_task{};
        }
    }
}

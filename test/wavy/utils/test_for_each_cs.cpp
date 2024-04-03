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
#include <glm/common.hpp>
#include <spdlog/spdlog.h>
#include <numeric>

#define THREADSAVE_CHECK_EQ(atomic_var, a, b) \
[&avar = atomic_var, aa = a, bb = b]() \
{ \
    bool expected = true; \
    avar.compare_exchange_strong(expected, aa == bb); \
    if (aa != bb) { \
        spdlog::error(#a "({}) != " #b "({})", aa, bb); \
    } \
}()

namespace wavy::utils
{
    namespace detail
    {
        template<class WorkGroupInfoType>
        struct cs_kernel_local_info
        {
            cs_kernel_local_info(const WorkGroupInfoType& winfo, const glm::uvec3& global_invocation_id,
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

    TEST_CASE("wavy::utils::emulate_compute_shader.simple execution", "")
    {
        bool single_thread_executed = false;
        auto kernel = [&single_thread_executed](work_group_info<void> winfo, glm::uvec3 local_invocation_id,
                                                glm::uvec3 global_invocation_id,
                                                unsigned local_invocation_index) -> coro::task<> {
            single_thread_executed = true;
            co_return;
        };

        emulate_compute_shader(glm::uvec3{1}, glm::uvec3{1}, kernel);

        CHECK(single_thread_executed == true);
    }

    class cs_test_fixture
    {
    public:
        cs_test_fixture()
            : simple_kernel{[this](work_group_info<void> winfo, glm::uvec3 local_invocation_id,
                                   glm::uvec3 global_invocation_id, unsigned local_invocation_index) -> coro::task<> {
                detail::cs_kernel_local_info local_info{winfo, global_invocation_id, local_invocation_id};

                THREADSAVE_CHECK_EQ(atomic_all_invocation_indices_correct, local_info.global_invocation_index,
                                    local_info.global_work_group_start_index + local_info.local_index_as_global_offset);

                global_thread_executed[std::array<std::size_t, 3>{global_invocation_id.x, global_invocation_id.y,
                                                                  global_invocation_id.z}] = 1;
                local_thread_counts_id[std::array<std::size_t, 3>{local_invocation_id.x, local_invocation_id.y,
                                                                  local_invocation_id.z}] += 1;
                local_thread_counts_index[local_invocation_index] += 1;
                co_return;
            }}
        {
        }

        void run_simple_test(const glm::uvec3& work_groups, const glm::uvec3& work_group_size)
        {
            const std::size_t work_groups_linear = work_groups.x * work_groups.y * work_groups.z;
            const std::size_t work_group_size_linear = work_group_size.x * work_group_size.y * work_group_size.z;

            std::vector<std::uint8_t> thread_executed_linear(work_groups_linear * work_group_size_linear, 0);
            global_thread_executed = std::mdspan(thread_executed_linear.data(), work_groups.x * work_group_size.x,
                                                 work_groups.y * work_group_size.y, work_groups.z * work_group_size.z);

            std::vector<std::atomic_size_t> local_thread_counts_linear(work_group_size_linear);
            for (auto& local_thread_count : local_thread_counts_linear) { local_thread_count.store(0); }
            local_thread_counts_id =
                std::mdspan(local_thread_counts_linear.data(), work_group_size.x, work_group_size.y, work_group_size.z);

            local_thread_counts_index = std::vector<std::atomic_size_t>(work_group_size_linear);
            for (auto& local_thread_count : local_thread_counts_index) { local_thread_count.store(0); }

            atomic_all_invocation_indices_correct = true;

            emulate_compute_shader(work_groups, work_group_size, simple_kernel);

            bool all_invocation_indices_correct = atomic_all_invocation_indices_correct.load();
            CHECK(all_invocation_indices_correct == true);

            for (const auto& executed : thread_executed_linear) { CHECK(executed == 1); }
            for (const auto& work_group_count_atomic : local_thread_counts_linear) {
                auto work_group_count = work_group_count_atomic.load();
                CHECK(work_group_count == work_groups_linear);
            }

            for (const auto& work_group_count_atomic : local_thread_counts_index) {
                auto work_group_count = work_group_count_atomic.load();
                CHECK(work_group_count == work_groups_linear);
            }
        }

    private:
        std::mdspan<std::uint8_t, std::dextents<std::size_t, 3>> global_thread_executed;
        std::mdspan<std::atomic_size_t, std::dextents<std::size_t, 3>> local_thread_counts_id;
        std::vector<std::atomic_size_t> local_thread_counts_index;
        std::atomic_bool atomic_all_invocation_indices_correct = true;
        std::function<coro::task<>(work_group_info<void>, glm::uvec3, glm::uvec3, unsigned)> simple_kernel;
    };

    TEST_CASE_METHOD(cs_test_fixture, "wavy::utils::emulate_compute_shader.multiple threads in one workgroup 1D", "")
    {
        run_simple_test(glm::uvec3{1}, glm::uvec3{100, 1, 1});
    }

    TEST_CASE_METHOD(cs_test_fixture , "wavy::utils::emulate_compute_shader.multiple threads in one workgroup 2D", "")
    {
        run_simple_test(glm::uvec3{1}, glm::uvec3{10, 10, 1});
    }

    TEST_CASE_METHOD(cs_test_fixture, "wavy::utils::emulate_compute_shader.multiple threads in one workgroup 3D", "")
    {
        run_simple_test(glm::uvec3{1}, glm::uvec3{5, 5, 5});
    }

    TEST_CASE_METHOD(cs_test_fixture,
                     "wavy::utils::emulate_compute_shader.multiple threads in multiple workgroups 1D/1D", "")
    {
        run_simple_test(glm::uvec3{100, 1, 1}, glm::uvec3{100, 1, 1});
    }

    TEST_CASE_METHOD(cs_test_fixture,
                     "wavy::utils::emulate_compute_shader.multiple threads in multiple workgroups 2D/2D", "")
    {
        run_simple_test(glm::uvec3{10, 10, 1}, glm::uvec3{10, 10, 1});
    }

    TEST_CASE_METHOD(cs_test_fixture,
                     "wavy::utils::emulate_compute_shader.multiple threads in multiple workgroups 3D/3D", "")
    {
        run_simple_test(glm::uvec3{5, 5, 5}, glm::uvec3{5, 5, 5});
    }

    TEST_CASE_METHOD(cs_test_fixture,
                     "wavy::utils::emulate_compute_shader.multiple threads in multiple workgroups 1D/3D", "")
    {
        run_simple_test(glm::uvec3{100, 1, 1}, glm::uvec3{5, 5, 5});
    }

    TEST_CASE_METHOD(cs_test_fixture,
                     "wavy::utils::emulate_compute_shader.multiple threads in multiple workgroups 3D/1D", "")
    {
        run_simple_test(glm::uvec3{5, 5, 5}, glm::uvec3{100, 1, 1});
    }

    TEST_CASE("wavy::utils::emulate_compute_shader.barriers", "")
    {
        constexpr glm::uvec3 work_groups{5, 2, 7};
        constexpr std::size_t work_groups_linear = work_groups.x * work_groups.y * work_groups.z;
        constexpr glm::uvec3 work_group_size{3, 6, 4};
        constexpr std::size_t work_group_size_linear = work_group_size.x * work_group_size.y * work_group_size.z;

        std::vector<std::uint8_t> thread_executed_linear(work_groups_linear * work_group_size_linear, 0);
        std::mdspan global_thread_executed(thread_executed_linear.data(), work_groups.x * work_group_size.x,
                                           work_groups.y * work_group_size.y, work_groups.z * work_group_size.z);

        auto kernel = [&global_thread_executed](work_group_info<void> winfo, glm::uvec3 local_invocation_id,
                                                glm::uvec3 global_invocation_id,
                                                unsigned local_invocation_index) -> coro::task<> {
            global_thread_executed[std::array<std::size_t, 3>{global_invocation_id.x, global_invocation_id.y,
                                                              global_invocation_id.z}] += 1;

            co_await std::suspend_always{};

            global_thread_executed[std::array<std::size_t, 3>{global_invocation_id.x, global_invocation_id.y,
                                                              global_invocation_id.z}] += 1;

            co_await std::suspend_always{};

            global_thread_executed[std::array<std::size_t, 3>{global_invocation_id.x, global_invocation_id.y,
                                                              global_invocation_id.z}] += 1;
        };

        emulate_compute_shader(work_groups, work_group_size, kernel);

        for (const auto& executed : thread_executed_linear) { CHECK(executed == 3); }
    }

    namespace test_helper {
        constexpr glm::uvec3 work_groups{5, 2, 7};
        constexpr std::size_t work_groups_linear = work_groups.x * work_groups.y * work_groups.z;
        constexpr glm::uvec3 work_group_size{3, 6, 4};
        constexpr std::size_t work_group_size_linear = work_group_size.x * work_group_size.y * work_group_size.z;

        constexpr std::size_t cluster_size = 4;

        struct shared_memory
        {
            std::vector<std::size_t> elements = std::vector<std::size_t>(work_group_size_linear, 0);
            std::size_t first_index = 0;
            std::size_t last_index = 0;
            std::atomic_bool count_correct = true;
        };

        using work_group_info = wavy::utils::work_group_info<shared_memory>;

        void store_indices(std::size_t local_invocation_index,
                           const detail::cs_kernel_local_info<work_group_info>& local_info, shared_memory& sm)
        {
            sm.elements[local_invocation_index] = local_info.global_invocation_index;

            if (local_invocation_index == 0) { sm.first_index = local_info.global_invocation_index; }
            if (local_invocation_index == local_info.work_group_size_linear - 1) {
                sm.last_index = local_info.global_invocation_index;
            }
        }

        std::size_t calc_cluster_sum(std::size_t local_invocation_index,
                                     const detail::cs_kernel_local_info<work_group_info>& local_info, shared_memory& sm,
                                     std::array<std::size_t, cluster_size>& indices_0)
        {
            std::size_t sum = 0;
            std::array<std::size_t, cluster_size> indices_1;
            if (local_invocation_index % cluster_size == 0) {
                for (std::size_t i = 0; i < cluster_size; ++i) {
                    indices_0[i] =
                        glm::min<std::size_t>(local_info.work_group_size_linear - 1, local_invocation_index + i);
                    indices_1[i] = local_info.work_group_size_linear - 1 - indices_0[i];
                    sum += sm.elements[indices_0[i]] + sm.elements[indices_1[i]];
                }
            }
            return sum;
        }

        void store_sum(std::size_t local_invocation_index, const work_group_info& winfo,
                       const std::array<std::size_t, cluster_size>& indices_0, std::size_t sum)
        {
            if (local_invocation_index % cluster_size == 0) {
                for (std::size_t i = 0; i < cluster_size; ++i) { winfo.shared_memory->elements[indices_0[i]] = sum; }
            }
        }

        void check_all_sums(std::size_t local_invocation_index, const work_group_info& winfo)
        {
            if (winfo.shared_memory->elements[local_invocation_index]
                != 4 * (winfo.shared_memory->first_index + winfo.shared_memory->last_index)) {
                bool expected = true;
                winfo.shared_memory->count_correct.compare_exchange_strong(expected, false);
            }
        }

        void propagate_checks(std::size_t local_invocation_index, const work_group_info& winfo,
                              std::mdspan<std::uint8_t, std::dextents<std::size_t, 3>> count_correct)
        {
            if (local_invocation_index == 0 && winfo.shared_memory->count_correct) {
                count_correct[std::array<std::size_t, 3>{winfo.work_group_id.x, winfo.work_group_id.y,
                                                         winfo.work_group_id.z}] = 1;
            }
        }
    }

    TEST_CASE("wavy::utils::emulate_compute_shader.shared memory", "")
    {

        std::vector<std::uint8_t> count_correct_linear(test_helper::work_groups_linear, 0);
        std::mdspan count_correct(count_correct_linear.data(), test_helper::work_groups.x, test_helper::work_groups.y,
                                  test_helper::work_groups.z);

        auto kernel = [&count_correct](test_helper::work_group_info winfo, glm::uvec3 local_invocation_id,
                                       glm::uvec3 global_invocation_id,
                                       unsigned local_invocation_index) -> coro::task<> {
            detail::cs_kernel_local_info local_info{winfo, global_invocation_id, local_invocation_id};
            test_helper::store_indices(local_invocation_index, local_info, *winfo.shared_memory);

            co_await std::suspend_always{};

            std::array<std::size_t, test_helper::cluster_size> indices_0;
            std::size_t sum =
                test_helper::calc_cluster_sum(local_invocation_index, local_info, *winfo.shared_memory, indices_0);

            co_await std::suspend_always{};

            store_sum(local_invocation_index, winfo, indices_0, sum);

            co_await std::suspend_always{};

            check_all_sums(local_invocation_index, winfo);

            co_await std::suspend_always{};

            propagate_checks(local_invocation_index, winfo, count_correct);
        };

        emulate_compute_shader<test_helper::shared_memory>(test_helper::work_groups, test_helper::work_group_size,
                                                           kernel);

        for (const auto& count_correct_work_group : count_correct_linear) { CHECK(count_correct_work_group == 1); }
    }
}

#include "../EmptyApp.hpp"

#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include <atomic>
#include <thread>
#include <vector>

TEST(ThreadPoolSpec, SingleWorkerShouldExecuteQueuedTasks)
{
    auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                    builder.WithThreadCount(1).WithArchetypeChunkCapacity(16).WithMaxEntities(64);
                });
            })
            .Build<EmptyApp>();

    auto threadPool = app->GetRootServiceProvider()->GetService<fr::ThreadPool>();

    std::atomic<int> hits { 0 };

    threadPool->StartWorkers();
    threadPool->AddTask([&hits] { hits.fetch_add(1); });
    threadPool->AddTask([&hits] { hits.fetch_add(1); });
    threadPool->WaitForAllTasks();
    threadPool->StopWorkers();

    ASSERT_EQ(hits.load(), 2);
}

TEST(ThreadPoolSpec, TasksQueuedWhileIdleShouldRunAfterStartWorkers)
{
    auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                    builder.WithThreadCount(1).WithArchetypeChunkCapacity(16).WithMaxEntities(64);
                });
            })
            .Build<EmptyApp>();

    auto threadPool = app->GetRootServiceProvider()->GetService<fr::ThreadPool>();

    std::atomic<int> hits { 0 };

    threadPool->StartWorkers();
    threadPool->StopWorkers();

    threadPool->AddTask([&hits] { hits.fetch_add(1); });
    threadPool->StartWorkers();
    threadPool->WaitForAllTasks();
    threadPool->StopWorkers();

    ASSERT_EQ(hits.load(), 1);
}

TEST(ThreadPoolSpec, ConcurrentResizeShouldIgnoreCallerWhileAlreadyResizing)
{
    auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                    builder.WithThreadCount(4).WithArchetypeChunkCapacity(16).WithMaxEntities(64);
                });
            })
            .Build<EmptyApp>();

    auto threadPool = app->GetRootServiceProvider()->GetService<fr::ThreadPool>();

    constexpr int kIterations = 64;
    constexpr int kCallers    = 8;

    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
        std::atomic<bool>        startGate { false };
        std::vector<std::thread> callers;
        callers.reserve(kCallers);

        for (int caller = 0; caller < kCallers; ++caller)
        {
            const auto threadCount = static_cast<std::uint32_t>(2 + ((iteration + caller) % 4));
            callers.emplace_back([threadPool, &startGate, threadCount] {
                while (!startGate.load(std::memory_order_acquire))
                {
                }
                threadPool->Resize(threadCount);
            });
        }

        startGate.store(true, std::memory_order_release);

        for (auto& caller : callers)
        {
            caller.join();
        }
    }

    std::atomic<int> hits { 0 };

    threadPool->StartWorkers();
    threadPool->AddTask([&hits] { hits.fetch_add(1); });
    threadPool->AddTask([&hits] { hits.fetch_add(1); });
    threadPool->WaitForAllTasks();
    threadPool->StopWorkers();

    ASSERT_EQ(hits.load(), 2);
}

TEST(ThreadPoolSpec, DestructorShouldJoinWorkersSleepingInIdle)
{
    constexpr int kIterations = 256;

    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
        auto app =
            skr::ApplicationBuilder()
                .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension& freyr) {
                    freyr.WithOptions([](fr::FreyrOptionsBuilder& builder) {
                        builder.WithThreadCount(4).WithArchetypeChunkCapacity(16).WithMaxEntities(
                            64);
                    });
                })
                .Build<EmptyApp>();

        auto threadPool = app->GetRootServiceProvider()->GetService<fr::ThreadPool>();

        threadPool->StartWorkers();
        threadPool->StopWorkers();
    }
}

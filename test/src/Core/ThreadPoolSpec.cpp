#include "../EmptyApp.hpp"

#include "gtest/gtest.h"

#include <Freyr/Freyr.hpp>

#include <atomic>

TEST(ThreadPoolSpec, SingleWorkerShouldExecuteQueuedTasks)
{
    auto app = skr::ApplicationBuilder()
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
    auto app = skr::ApplicationBuilder()
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

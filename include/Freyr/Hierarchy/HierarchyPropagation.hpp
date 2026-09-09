#pragma once

#include "Freyr/Core/FreyrOptions.hpp"
#include "Freyr/Core/ThreadPool.hpp"
#include "Freyr/Hierarchy/HierarchyManager.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationMode.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationPolicy.hpp"
#include "Freyr/Hierarchy/HierarchyWorkQueue.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace FREYR_NAMESPACE
{
    template <HierarchyPropagationPolicy Policy>
    void PropagateDescendants(HierarchyManager&    hierarchy,
                              ComponentManager&    components,
                              Policy&              policy,
                              Entity               parent,
                              std::vector<Entity>& outbox,
                              HierarchyWorkQueue&  queue,
                              std::size_t          maxDepth,
                              bool                 dirtyOnly)
    {
        Entity current = parent;

        for (std::size_t depth = 1; depth <= maxDepth; ++depth)
        {
            const auto children = hierarchy.Children(current);
            if (children.empty())
                break;

            Entity lastBranch = NullEntity;

            for (const Entity child : children)
            {
                if (dirtyOnly && !hierarchy.IsDirty(child))
                    continue;

                policy.Propagate(components, current, child);

                if (hierarchy.HasChildren(child) && policy.HasChildrenInterest(components, child))
                {
                    outbox.push_back(child);
                    lastBranch = child;
                }
            }

            if (depth >= maxDepth || lastBranch == NullEntity)
                break;

            current = lastBranch;
            outbox.pop_back();

            if (outbox.size() >= HierarchyWorkQueue::ChunkSize)
                queue.SendBatches(outbox);
        }
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagationWorker(HierarchyManager&   hierarchy,
                           ComponentManager&   components,
                           Policy&             policy,
                           HierarchyWorkQueue& queue,
                           bool                dirtyOnly)
    {
        std::vector<Entity> outbox;
        outbox.reserve(HierarchyWorkQueue::ChunkSize * 2);
        std::vector<Entity> tasks;
        tasks.reserve(HierarchyWorkQueue::ChunkSize);

        for (;;)
        {
            const auto claim = queue.TryClaim(tasks);
            if (claim == HierarchyWorkQueue::ClaimResult::Done)
                break;
            if (claim == HierarchyWorkQueue::ClaimResult::IdleRetry)
            {
                std::this_thread::yield();
                continue;
            }

            for (const Entity parent : tasks)
            {
                PropagateDescendants(hierarchy, components, policy, parent, outbox, queue, 10'000,
                                     dirtyOnly);
            }

            queue.SendBatches(outbox);
            queue.FinishBatch();
            tasks.clear();
        }
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagateForestWorkSharing(HierarchyManager&          hierarchy,
                                    ComponentManager&          components,
                                    std::uint64_t              threadCount,
                                    Policy                     policy,
                                    const std::vector<Entity>& rootsWithChildren,
                                    bool                       dirtyOnly)
    {
        if (rootsWithChildren.empty())
            return;

        HierarchyWorkQueue  queue;
        std::vector<Entity> outbox;
        outbox.reserve(HierarchyWorkQueue::ChunkSize * 2);

        for (const Entity root : rootsWithChildren)
        {
            if (dirtyOnly && !hierarchy.IsDirty(root))
                continue;
            PropagateDescendants(hierarchy, components, policy, root, outbox, queue, 1, dirtyOnly);
        }

        queue.SendBatches(outbox);

        if (!queue.HasPendingTasks())
            return;

        const auto workerCount =
            static_cast<std::uint64_t>(std::max<std::uint64_t>(1, threadCount));

        std::vector<std::thread> workers;
        workers.reserve(workerCount > 0 ? workerCount - 1 : 0);
        for (std::uint64_t i = 1; i < workerCount; ++i)
        {
            workers.emplace_back([&hierarchy, &components, &policy, &queue, dirtyOnly] {
                PropagationWorker(hierarchy, components, policy, queue, dirtyOnly);
            });
        }

        PropagationWorker(hierarchy, components, policy, queue, dirtyOnly);

        for (auto& worker : workers)
            worker.join();
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagateForestLevelSync(HierarchyManager& hierarchy,
                                  ComponentManager& components,
                                  std::uint64_t     threadCount,
                                  Policy            policy,
                                  bool              dirtyOnly)
    {
        hierarchy.EnsureDepthBuckets();
        const auto maxDepth = hierarchy.MaxDepth();
        if (maxDepth == 0)
            return;

        const auto workers =
            static_cast<std::uint64_t>(std::max<std::uint64_t>(1, threadCount));
        constexpr std::size_t kGrain = 512;

        for (std::uint16_t depth = 1; depth <= maxDepth; ++depth)
        {
            const auto span = hierarchy.EntitiesAtDepth(depth);
            if (span.empty())
                continue;

            std::atomic<std::size_t> next { 0 };
            const auto               total = span.size();

            const auto workerFn = [&] {
                for (;;)
                {
                    const std::size_t begin = next.fetch_add(kGrain, std::memory_order_relaxed);
                    if (begin >= total)
                        break;
                    const std::size_t end = std::min(begin + kGrain, total);
                    for (std::size_t i = begin; i < end; ++i)
                    {
                        const Entity entity = span[i];
                        if (dirtyOnly && !hierarchy.IsDirty(entity))
                            continue;
                        const Entity parent = hierarchy.GetParent(entity);
                        if (parent == NullEntity)
                            continue;
                        policy.Propagate(components, parent, entity);
                    }
                }
            };

            std::vector<std::thread> pool;
            pool.reserve(workers > 0 ? workers - 1 : 0);
            for (std::uint64_t w = 1; w < workers; ++w)
                pool.emplace_back(workerFn);

            workerFn();

            for (auto& thread : pool)
                thread.join();
        }
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagateForest(HierarchyManager&          hierarchy,
                         ComponentManager&          components,
                         ThreadPool&                threadPool,
                         std::uint64_t              threadCount,
                         Policy                     policy,
                         const std::vector<Entity>& rootsWithChildren,
                         HierarchyPropagationMode   mode = HierarchyPropagationMode::WorkSharing)
    {
        (void) threadPool;
        const bool dirtyOnly = hierarchy.HasAnyDirty();

        if (mode == HierarchyPropagationMode::LevelSync)
        {
            PropagateForestLevelSync(hierarchy, components, threadCount, policy, dirtyOnly);
        }
        else
        {
            PropagateForestWorkSharing(hierarchy, components, threadCount, policy, rootsWithChildren,
                                       dirtyOnly);
        }

        if (dirtyOnly)
            hierarchy.ClearDirty();
    }
} // namespace FREYR_NAMESPACE

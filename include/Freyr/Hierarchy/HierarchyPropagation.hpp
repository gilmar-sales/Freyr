#pragma once

#include "Freyr/Core/FreyrOptions.hpp"
#include "Freyr/Core/Processor.hpp"
#include "Freyr/Core/ThreadPool.hpp"
#include "Freyr/Hierarchy/HierarchyManager.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationMode.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationPolicy.hpp"
#include "Freyr/Hierarchy/HierarchyWorkQueue.hpp"

#include <algorithm>
#include <atomic>
#include <span>
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
                              std::size_t          maxDepth)
    {
        Entity current = parent;

        for (std::size_t depth = 1; depth <= maxDepth; ++depth)
        {
            Entity lastBranch = NullEntity;

            hierarchy.ForEachChild(current, [&](Entity child) {
                policy.Propagate(components, current, child);

                if (hierarchy.HasChildren(child) && policy.HasChildrenInterest(components, child))
                {
                    outbox.push_back(child);
                    lastBranch = child;
                }
            });

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
                           HierarchyWorkQueue& queue)
    {
        std::vector<Entity> outbox;
        outbox.reserve(HierarchyWorkQueue::ChunkSize * 2);
        std::vector<Entity> tasks;
        tasks.reserve(HierarchyWorkQueue::ChunkSize);

        constexpr int kIdleSpins = 128;

        for (;;)
        {
            auto claim = queue.TryClaim(tasks);
            if (claim == HierarchyWorkQueue::ClaimResult::IdleRetry)
            {
                for (int i = 0; i < kIdleSpins; ++i)
                    Processor::Pause();
                claim = queue.TryClaim(tasks);
                if (claim == HierarchyWorkQueue::ClaimResult::IdleRetry)
                {
                    std::this_thread::yield();
                    continue;
                }
            }

            if (claim == HierarchyWorkQueue::ClaimResult::Done)
                break;

            for (const Entity parent : tasks)
                PropagateDescendants(hierarchy, components, policy, parent, outbox, queue, 10'000);

            queue.SendBatches(outbox);
            queue.FinishBatch();
            tasks.clear();
        }
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagateForestWorkSharing(HierarchyManager&       hierarchy,
                                    ComponentManager&       components,
                                    ThreadPool&             threadPool,
                                    std::uint64_t           threadCount,
                                    Policy                  policy,
                                    std::span<const Entity> parents)
    {
        if (parents.empty())
            return;

        HierarchyWorkQueue  queue;
        std::vector<Entity> outbox;
        outbox.reserve(HierarchyWorkQueue::ChunkSize * 2);

        for (const Entity parent : parents)
            PropagateDescendants(hierarchy, components, policy, parent, outbox, queue, 1);

        queue.SendBatches(outbox);

        if (!queue.HasPendingTasks())
            return;

        const auto workerCount =
            static_cast<std::uint64_t>(std::max<std::uint64_t>(1, threadCount));
        const auto pooledWorkers = workerCount > 1 ? workerCount - 1 : 0;

        for (std::uint64_t i = 0; i < pooledWorkers; ++i)
        {
            threadPool.AddTask(Task { [&hierarchy, &components, &policy, &queue] {
                PropagationWorker(hierarchy, components, policy, queue);
            } });
        }

        PropagationWorker(hierarchy, components, policy, queue);

        if (pooledWorkers > 0)
            threadPool.WaitForAllTasks();
    }

    template <typename TFunc>
    void ParallelForEntities(ThreadPool&             threadPool,
                             std::uint64_t           threadCount,
                             std::span<const Entity> entities,
                             TFunc&&                 func)
    {
        constexpr std::size_t kGrain = 64;

        const auto total = entities.size();
        if (total == 0)
            return;

        const auto workers = static_cast<std::uint64_t>(std::max<std::uint64_t>(1, threadCount));
        if (workers == 1 || total <= kGrain)
        {
            for (const Entity entity : entities)
                func(entity);
            return;
        }

        std::atomic<std::size_t> next { 0 };

        const auto workerFn = [&] {
            for (;;)
            {
                const std::size_t begin = next.fetch_add(kGrain, std::memory_order_relaxed);
                if (begin >= total)
                    break;
                const std::size_t end = std::min(begin + kGrain, total);
                for (std::size_t i = begin; i < end; ++i)
                    func(entities[i]);
            }
        };

        const auto chunks        = static_cast<std::uint64_t>((total + kGrain - 1) / kGrain);
        const auto pooledWorkers = std::min<std::uint64_t>(workers, chunks) - 1;
        for (std::uint64_t w = 0; w < pooledWorkers; ++w)
            threadPool.AddTask(Task { workerFn });

        workerFn();

        if (pooledWorkers > 0)
            threadPool.WaitForAllTasks();
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagateForestLevelSync(HierarchyManager& hierarchy,
                                  ComponentManager& components,
                                  ThreadPool&       threadPool,
                                  std::uint64_t     threadCount,
                                  Policy            policy)
    {
        hierarchy.EnsureDepthBuckets();
        const auto maxDepth = hierarchy.MaxDepth();

        for (std::uint16_t depth = 1; depth <= maxDepth; ++depth)
        {
            ParallelForEntities(threadPool, threadCount, hierarchy.EntitiesAtDepth(depth),
                                [&](Entity entity) {
                                    const Entity parent = hierarchy.GetParent(entity);
                                    if (parent != NullEntity)
                                        policy.Propagate(components, parent, entity);
                                });
        }
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagateSubtreesLevelSync(HierarchyManager&       hierarchy,
                                    ComponentManager&       components,
                                    ThreadPool&             threadPool,
                                    std::uint64_t           threadCount,
                                    Policy                  policy,
                                    std::span<const Entity> parents)
    {
        std::vector<Entity> frontier(parents.begin(), parents.end());
        std::vector<Entity> level;

        while (!frontier.empty())
        {
            level.clear();
            for (const Entity parent : frontier)
                hierarchy.ForEachChild(parent, [&](Entity child) { level.push_back(child); });

            ParallelForEntities(threadPool, threadCount, level, [&](Entity entity) {
                policy.Propagate(components, hierarchy.GetParent(entity), entity);
            });

            frontier.clear();
            for (const Entity entity : level)
            {
                if (hierarchy.HasChildren(entity) && policy.HasChildrenInterest(components, entity))
                    frontier.push_back(entity);
            }
        }
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagateForest(HierarchyManager&        hierarchy,
                         ComponentManager&        components,
                         ThreadPool&              threadPool,
                         std::uint64_t            threadCount,
                         Policy                   policy,
                         std::span<const Entity>  rootsWithChildren,
                         HierarchyPropagationMode mode = HierarchyPropagationMode::WorkSharing)
    {
        if (mode == HierarchyPropagationMode::LevelSync)
            PropagateForestLevelSync(hierarchy, components, threadPool, threadCount, policy);
        else
            PropagateForestWorkSharing(hierarchy, components, threadPool, threadCount, policy,
                                       rootsWithChildren);
    }

    template <HierarchyPropagationPolicy Policy>
    void PropagateDirty(HierarchyManager&        hierarchy,
                        ComponentManager&        components,
                        ThreadPool&              threadPool,
                        std::uint64_t            threadCount,
                        Policy                   policy,
                        std::vector<Entity>&     heads,
                        std::vector<Entity>&     parents,
                        HierarchyPropagationMode mode = HierarchyPropagationMode::WorkSharing)
    {
        using Local = typename Policy::Local;
        using World = typename Policy::World;

        heads.clear();
        parents.clear();
        hierarchy.CollectDirtyHeads(heads);

        for (const Entity head : heads)
        {
            if (!components.HasComponents<Local, World>(head))
                continue;

            const Entity parent = hierarchy.GetParent(head);
            if (parent == NullEntity)
                policy.OnRoot(components, head);
            else
                policy.Propagate(components, parent, head);

            if (hierarchy.HasChildren(head) && policy.HasChildrenInterest(components, head))
                parents.push_back(head);
        }

        if (mode == HierarchyPropagationMode::LevelSync)
            PropagateSubtreesLevelSync(hierarchy, components, threadPool, threadCount, policy,
                                       parents);
        else
            PropagateForestWorkSharing(hierarchy, components, threadPool, threadCount, policy,
                                       parents);

        hierarchy.ClearDirty<Local>();
    }
} // namespace FREYR_NAMESPACE

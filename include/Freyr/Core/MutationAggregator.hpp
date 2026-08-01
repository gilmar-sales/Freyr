#pragma once

#include "Freyr/Core/Mutation.hpp"
#include "Freyr/Core/ThreadPool.hpp"

namespace FREYR_NAMESPACE
{

    class MutationAggregator
    {
      public:
        explicit MutationAggregator(const skr::Arc<ComponentManager>& componentManager,
                                    const skr::Arc<ThreadPool>&       taskManager) :
            mComponentManager(componentManager), mThreadPool(taskManager)
        {
        }

        void Reset()
        {
            mThreadPool->WaitForAllTasks();
            mPendingTasks.clear();
        }

        void Schedule(PendingMutation&& pendingMutation)
        {
            mPendingTasks.emplace_back(std::move(pendingMutation));
        }

        void Flush()
        {
            FREYR_TRACE_BEGIN("FREYR", "MutationAggregator: Flush",
                              perfetto::Track(0, perfetto::ProcessTrack::Current()));

            mComponentManager->ForEachArchetype([&](Archetype* archetype) {
                auto matchedTasks = std::vector<PendingMutation*>();
                {
                    for (auto& pendingTask : mPendingTasks)
                    {
                        if (pendingTask.filter.MatchArchetype(archetype))
                        {
                            matchedTasks.push_back(&pendingTask);
                        }
                    }
                }

                if (matchedTasks.empty())
                    return;

                archetype->ForEachChunk([matchedTasks](ArchetypeChunk* chunk) {
                    chunk->EnqueueTask([matchedTasks, chunk] {
                        for (auto* matched : matchedTasks)
                        {
                            matched->action(*chunk);
                        }
                    });
                });
            });

            Reset();
            FREYR_TRACE_END("FREYR", perfetto::Track(0));
        }

        size_t GetScheduledTaskCount() const { return mPendingTasks.size(); }

      private:
        std::vector<PendingMutation> mPendingTasks;
        skr::Arc<ComponentManager>        mComponentManager;
        skr::Arc<ThreadPool>              mThreadPool;
    };

} // namespace FREYR_NAMESPACE
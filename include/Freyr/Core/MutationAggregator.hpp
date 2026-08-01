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

            if (mPendingTasks.empty())
            {
                FREYR_TRACE_END("FREYR", perfetto::Track(0));
                return;
            }

            auto pending =
                skr::MakeArc<std::vector<PendingMutation>>(std::move(mPendingTasks));
            mPendingTasks.clear();

            mThreadPool->StartWorkers();

            mComponentManager->ForEachArchetype([&](Archetype* archetype) {
                std::vector<size_t> matchedIndexes;
                matchedIndexes.reserve(pending->size());

                for (size_t index = 0; index < pending->size(); ++index)
                {
                    if ((*pending)[index].filter.MatchArchetype(archetype))
                        matchedIndexes.push_back(index);
                }

                if (matchedIndexes.empty())
                    return;

                archetype->ForEachChunk([pending, matchedIndexes](ArchetypeChunk* chunk) {
                    chunk->EnqueueTask([pending, matchedIndexes, chunk] {
                        for (const auto index : matchedIndexes)
                        {
                            (*pending)[index].action(*chunk);
                        }
                    });
                });
            });

            mComponentManager->ForEachArchetype(
                [](Archetype* archetype) { archetype->StartTasks(); });
            mThreadPool->WaitForAllTasks();

            FREYR_TRACE_END("FREYR", perfetto::Track(0));
        }

        size_t GetScheduledTaskCount() const { return mPendingTasks.size(); }

      private:
        std::vector<PendingMutation> mPendingTasks;
        skr::Arc<ComponentManager>   mComponentManager;
        skr::Arc<ThreadPool>         mThreadPool;
    };

} // namespace FREYR_NAMESPACE

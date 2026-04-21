#pragma once

#include "Freyr/Core/Query.hpp"
#include "Freyr/Core/TaskManager.hpp"

namespace FREYR_NAMESPACE
{

    class QueryAggregator
    {
      public:
        explicit QueryAggregator(const Ref<ComponentManager>& componentManager, const Ref<TaskManager>& taskManager) :
            mComponentManager(componentManager), mTaskManager(taskManager)
        {
        }

        void Reset() { mPendingTasks.clear(); }

        void Schedule(const PendingQuery&& pendingQuery) { mPendingTasks.emplace_back(pendingQuery); }

        void Flush()
        {
            FREYR_TRACE_BEGIN("FREYR", "QueryAggregator: Flush", perfetto::Track(0, perfetto::ProcessTrack::Current()));

            mComponentManager->ForEachArchetype([&](Archetype* archetype) {
                auto matchedTasks = skr::MakeRef<std::vector<PendingQuery*>>();

                for (auto& pendingTask : mPendingTasks)
                {
                    if (pendingTask.filter.MatchArchetype(archetype))
                    {
                        matchedTasks->push_back(&pendingTask);
                    }
                }

                archetype->ForEachChunk([&, matchedTasks](ArchetypeChunk* chunk) {
                    mTaskManager->AddTask([matchedTasks, chunk] {
                        for (const auto* matched : *matchedTasks)
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
        std::vector<PendingQuery> mPendingTasks;
        Ref<ComponentManager>     mComponentManager;
        Ref<TaskManager>          mTaskManager;
    };

} // namespace FREYR_NAMESPACE
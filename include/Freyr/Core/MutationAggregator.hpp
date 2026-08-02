#pragma once

#include "Freyr/Core/Mutation.hpp"
#include "Freyr/Core/ThreadPool.hpp"

namespace FREYR_NAMESPACE
{

    class MutationAggregator
    {
      public:
        explicit MutationAggregator(const skr::Arc<ComponentManager>& componentManager,
                                    const skr::Arc<ThreadPool>&       taskManager);

        void Reset();

        void Schedule(PendingMutation&& pendingMutation);

        void Flush();

        [[nodiscard]] size_t GetScheduledTaskCount() const;

      private:
        std::vector<PendingMutation> mPendingTasks;
        skr::Arc<ComponentManager>   mComponentManager;
        skr::Arc<ThreadPool>         mThreadPool;
    };

} // namespace FREYR_NAMESPACE

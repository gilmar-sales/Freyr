#include "Freyr/Core/MutationAggregator.hpp"

#include "Freyr/Core/Profiling.hpp"

namespace FREYR_NAMESPACE
{
    MutationAggregator::MutationAggregator(const skr::Arc<ComponentManager>& componentManager,
                                           const skr::Arc<ThreadPool>&       taskManager) :
        mComponentManager(componentManager), mThreadPool(taskManager)
    {
    }

    void MutationAggregator::Reset()
    {
        mThreadPool->WaitForAllTasks();
        mPendingTasks.clear();
    }

    void MutationAggregator::Schedule(PendingMutation&& pendingMutation)
    {
        mPendingTasks.emplace_back(std::move(pendingMutation));
    }

    void MutationAggregator::Flush()
    {
        FREYR_TRACE_BEGIN("FREYR", "MutationAggregator: Flush");

        if (mPendingTasks.empty())
        {
            FREYR_TRACE_END("FREYR");
            return;
        }

        auto pending = skr::MakeArc<std::vector<PendingMutation>>(std::move(mPendingTasks));
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
                    if (matchedIndexes.size() == 1)
                    {
                        (*pending)[matchedIndexes[0]].run(*chunk);
                        return;
                    }

                    const size_t count = chunk->Count();
                    if (count == 0)
                        return;

                    constexpr size_t kMaxMatched = 64;
                    constexpr size_t kStackBytes = 4096;

                    if (matchedIndexes.size() > kMaxMatched)
                    {
                        for (const auto mutationIndex : matchedIndexes)
                        {
                            (*pending)[mutationIndex].run(*chunk);
                        }
                        return;
                    }

                    size_t bytes = 0;
                    for (const auto mutationIndex : matchedIndexes)
                    {
                        const auto bindingSize = (*pending)[mutationIndex].bindingSize;
                        bytes += (bindingSize + alignof(std::max_align_t) - 1) &
                                 ~(alignof(std::max_align_t) - 1);
                    }

                    alignas(std::max_align_t) std::byte stackStorage[kStackBytes];
                    std::unique_ptr<std::byte[]>       heapStorage;
                    std::byte*                         storage = stackStorage;
                    if (bytes > kStackBytes)
                    {
                        heapStorage = std::make_unique_for_overwrite<std::byte[]>(bytes);
                        storage     = heapStorage.get();
                    }

                    void* bindings[kMaxMatched];
                    void (*applies[kMaxMatched])(void*, size_t);

                    size_t offset = 0;
                    for (size_t matched = 0; matched < matchedIndexes.size(); ++matched)
                    {
                        auto&      mutation    = (*pending)[matchedIndexes[matched]];
                        const auto bindingSize = mutation.bindingSize;
                        void*      binding     = storage + offset;
                        mutation.bind(*chunk, mutation.actionState.get(), binding);
                        bindings[matched] = binding;
                        applies[matched]  = mutation.applyBound;
                        offset += (bindingSize + alignof(std::max_align_t) - 1) &
                                  ~(alignof(std::max_align_t) - 1);
                    }

                    const size_t matchedCount = matchedIndexes.size();
                    for (size_t index = 0; index < count; ++index)
                    {
                        for (size_t matched = 0; matched < matchedCount; ++matched)
                        {
                            applies[matched](bindings[matched], index);
                        }
                    }
                });
            });
        });

        mComponentManager->ForEachArchetype(
            [](Archetype* archetype) { archetype->StartTasks(); });
        mThreadPool->WaitForAllTasks();

        FREYR_TRACE_END("FREYR");
    }

    size_t MutationAggregator::GetScheduledTaskCount() const
    {
        return mPendingTasks.size();
    }
} // namespace FREYR_NAMESPACE

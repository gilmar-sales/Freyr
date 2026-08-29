#include "Freyr/Core/MutationAggregator.hpp"

#include "Freyr/Core/Profiling.hpp"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <vector>

namespace FREYR_NAMESPACE
{
    namespace
    {

        constexpr std::size_t kBindingStackBytes = 4096;

        using BoundMutationApply = void (*)(void*, std::size_t);

        std::size_t AlignBindingSize(const std::size_t bindingSize)
        {
            return (bindingSize + alignof(std::max_align_t) - 1) & ~(alignof(std::max_align_t) - 1);
        }

        void RunSingleMutation(ArchetypeChunk& chunk, PendingMutation& mutation)
        {
            mutation.run(chunk);
        }

        void RunBatchedMutations(ArchetypeChunk&                 chunk,
                                 std::vector<PendingMutation>&   pending,
                                 const std::vector<std::size_t>& matchedIndexes)
        {
            const std::size_t count = chunk.Count();
            if (count == 0)
                return;

            const std::size_t matchedCount = matchedIndexes.size();

            std::size_t bytes = 0;
            for (const auto mutationIndex : matchedIndexes)
                bytes += AlignBindingSize(pending[mutationIndex].bindingSize);

            alignas(std::max_align_t) std::byte stackStorage[kBindingStackBytes];
            std::unique_ptr<std::byte[]>        heapStorage;
            std::byte*                          storage = stackStorage;
            if (bytes > kBindingStackBytes)
            {
                heapStorage = std::make_unique_for_overwrite<std::byte[]>(bytes);
                storage     = heapStorage.get();
            }

            auto bindings = std::vector<void*>(matchedCount);
            auto applies  = std::vector<BoundMutationApply>(matchedCount);

            std::size_t offset = 0;
            for (std::size_t matched = 0; matched < matchedCount; ++matched)
            {
                auto& mutation = pending[matchedIndexes[matched]];
                void* binding  = storage + offset;
                mutation.bind(chunk, mutation.actionState.get(), binding);
                bindings[matched] = binding;
                applies[matched]  = mutation.applyBound;
                offset += AlignBindingSize(mutation.bindingSize);
            }

            for (std::size_t index = 0; index < count; ++index)
            {
                for (std::size_t matched = 0; matched < matchedCount; ++matched)
                    applies[matched](bindings[matched], index);
            }
        }

        void DispatchChunkMutations(ArchetypeChunk&                 chunk,
                                    std::vector<PendingMutation>&   pending,
                                    const std::vector<std::size_t>& matchedIndexes)
        {
            if (matchedIndexes.size() == 1)
            {
                RunSingleMutation(chunk, pending[matchedIndexes[0]]);
                return;
            }

            RunBatchedMutations(chunk, pending, matchedIndexes);
        }

        std::vector<std::size_t> CollectMatchingMutationIndexes(
            const std::vector<PendingMutation>& pending,
            const Archetype*                    archetype,
            const std::unordered_map<Signature, std::vector<std::size_t>, SignatureHash>&
                                            pendingByIncludeSignature,
            const std::vector<std::size_t>& pendingWithEmptyInclude)
        {
            std::vector<std::size_t> matchedIndexes;

            for (const auto index : pendingWithEmptyInclude)
            {
                if (pending[index].filter.MatchArchetype(archetype))
                    matchedIndexes.push_back(index);
            }

            const auto& archetypeSignature = archetype->GetSignature();

            for (const auto& [includeSignature, indices] : pendingByIncludeSignature)
            {
                if (!includeSignature.Match(archetypeSignature))
                    continue;

                for (const auto index : indices)
                {
                    if (pending[index].filter.MatchArchetype(archetype))
                        matchedIndexes.push_back(index);
                }
            }

            std::ranges::sort(matchedIndexes);
            return matchedIndexes;
        }

        void CollectFlushCandidateArchetypes(
            const ComponentManager& componentManager,
            const std::unordered_map<Signature, std::vector<std::size_t>, SignatureHash>&
                                      pendingByIncludeSignature,
            const std::vector<std::size_t>& pendingWithEmptyInclude,
            std::vector<Archetype*>&        candidates)
        {
            if (!pendingWithEmptyInclude.empty())
            {
                componentManager.ForEachArchetype([&](Archetype* archetype) {
                    candidates.push_back(archetype);
                });
                return;
            }

            std::unordered_set<Archetype*> seen;
            for (const auto& [includeSignature, _] : pendingByIncludeSignature)
            {
                for (Archetype* archetype :
                     componentManager.ArchetypesMatchingInclude(includeSignature))
                {
                    if (seen.insert(archetype).second)
                        candidates.push_back(archetype);
                }
            }
        }

    } // namespace

    MutationAggregator::MutationAggregator(const skr::Arc<ComponentManager>& componentManager,
                                           const skr::Arc<ThreadPool>&       taskManager) :
        mComponentManager(componentManager), mThreadPool(taskManager)
    {
    }

    void MutationAggregator::Reset()
    {
        mThreadPool->WaitForAllTasks();
        mPendingTasks.clear();
        mPendingByIncludeSignature.clear();
        mPendingWithEmptyInclude.clear();
    }

    void MutationAggregator::Schedule(PendingMutation&& pendingMutation)
    {
        const auto index = mPendingTasks.size();
        mPendingTasks.emplace_back(std::move(pendingMutation));

        const auto& includeSignature = mPendingTasks.back().filter.IncludeSignature();
        if (includeSignature.IsEmpty())
            mPendingWithEmptyInclude.push_back(index);
        else
            mPendingByIncludeSignature[includeSignature].push_back(index);
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
        auto pendingByIncludeSignature = std::move(mPendingByIncludeSignature);
        auto pendingWithEmptyInclude   = std::move(mPendingWithEmptyInclude);

        mPendingTasks.clear();
        mPendingByIncludeSignature.clear();
        mPendingWithEmptyInclude.clear();

        mThreadPool->StartWorkers();

        std::vector<Archetype*> candidateArchetypes;
        candidateArchetypes.reserve(mComponentManager->ArchetypeCount());
        CollectFlushCandidateArchetypes(*mComponentManager,
                                        pendingByIncludeSignature,
                                        pendingWithEmptyInclude,
                                        candidateArchetypes);

        for (Archetype* archetype : candidateArchetypes)
        {
            auto matchedIndexes = CollectMatchingMutationIndexes(*pending,
                                                                 archetype,
                                                                 pendingByIncludeSignature,
                                                                 pendingWithEmptyInclude);

            if (matchedIndexes.empty())
                continue;

            const auto sharedMatchedIndexes =
                skr::MakeArc<std::vector<std::size_t>>(std::move(matchedIndexes));

            archetype->ForEachChunk([pending, sharedMatchedIndexes](ArchetypeChunk* chunk) {
                chunk->EnqueueTask([pending, sharedMatchedIndexes, chunk] {
                    DispatchChunkMutations(*chunk, *pending, *sharedMatchedIndexes);
                });
            });

            archetype->StartTasks();
        }

        mThreadPool->WaitForAllTasks();

        FREYR_TRACE_END("FREYR");
    }

    size_t MutationAggregator::GetScheduledTaskCount() const
    {
        return mPendingTasks.size();
    }
} // namespace FREYR_NAMESPACE

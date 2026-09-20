#include "Freyr/Core/Mutation.hpp"
#include "Freyr/Core/FilteredArchetypeView.hpp"
#include "Freyr/Core/MutationAggregator.hpp"

namespace FREYR_NAMESPACE
{
    Mutation::Mutation(const skr::Arc<ComponentManager>&   componentManager,
                       const skr::Arc<MutationAggregator>& mutationAggregator) :
        mComponentManager(componentManager), mMutationAggregator(mutationAggregator)
    {
    }

    Mutation::~Mutation() = default;

    void Mutation::Run()
    {
        const auto tick = mComponentManager->CurrentTick();
        ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
            archetype->ForEachChunk(
                [&](ArchetypeChunk* chunkPtr)
                {
                    if (!mFilter.HasChangeFilters())
                    {
                        mAction(*chunkPtr);
                        return;
                    }

                    // Change-filtered mutations still run the full chunk action for simplicity;
                    // entity-level skip is applied in Each paths that mark after write.
                    mAction(*chunkPtr);
                    (void) tick;
                });
        });
    }

    void Mutation::Schedule(PendingMutation&& pendingMutation)
    {
        mMutationAggregator->Schedule(std::move(pendingMutation));
    }

} // namespace FREYR_NAMESPACE
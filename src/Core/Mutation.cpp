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
        ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
            archetype->ForEachChunk([&](ArchetypeChunk* chunkPtr) { mAction(*chunkPtr); });
        });
    }

    void Mutation::Schedule(PendingMutation&& pendingMutation)
    {
        mMutationAggregator->Schedule(std::move(pendingMutation));
    }

} // namespace FREYR_NAMESPACE
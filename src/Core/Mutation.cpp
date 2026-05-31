#include "Freyr/Core/Mutation.hpp"
#include "Freyr/Core/MutationAggregator.hpp"

namespace FREYR_NAMESPACE
{
    Mutation::Mutation(const Ref<ComponentManager>&   componentManager,
                       const Ref<MutationAggregator>& mutationAggregator) :
        mComponentManager(componentManager), mMutationAggregator(mutationAggregator)
    {
    }

    Mutation::~Mutation() = default;

    void Mutation::Run()
    {
        mComponentManager->ForEachArchetype([&](Archetype* archetype) {
            if (!mFilter.MatchArchetype(archetype))
                return;

            archetype->ForEachChunk([&](ArchetypeChunk* chunkPtr) { mAction(*chunkPtr); });
        });
    }

    void Mutation::Schedule()
    {
        mMutationAggregator->Schedule(
            PendingMutation { .filter = mFilter, .action = std::move(mAction) });
    }

} // namespace FREYR_NAMESPACE
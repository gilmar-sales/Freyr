#include "Freyr/Core/Query.hpp"

#include "Freyr/Core/QueryAggregator.hpp"

namespace FREYR_NAMESPACE
{
    Query::Query(const Ref<ComponentManager>& componentManager, const Ref<QueryAggregator>& queryAggregator) :
        mComponentManager(componentManager), mQueryAggregator(queryAggregator)
    {
    }

    Query::~Query() = default;

    void Query::Run()
    {
        mComponentManager->ForEachArchetype([&](Archetype* archetype) {
            if (!mQueryFilter.MatchArchetype(archetype))
                return;

            archetype->ForEachChunk([&](ArchetypeChunk* chunkPtr) { mAction(*chunkPtr); });
        });
    }

    void Query::Schedule()
    {
        mQueryAggregator->Schedule(PendingQuery { .filter = mQueryFilter, .action = std::move(mAction) });
    }

} // namespace FREYR_NAMESPACE
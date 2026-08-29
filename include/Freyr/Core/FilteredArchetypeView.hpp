#pragma once

#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/Filter.hpp"

#include <cstddef>
#include <vector>

namespace FREYR_NAMESPACE
{

    template <typename Fn>
    void ForEachMatchingArchetype(const ComponentManager& componentManager,
                                  const Filter&           filter,
                                  Fn&&                    function)
    {
        componentManager.ForEachMatchingArchetype(filter, std::forward<Fn>(function));
    }

    template <typename PendingEntry, typename Fn>
    void ForEachArchetypeWithMatchingPending(const ComponentManager&          componentManager,
                                             const std::vector<PendingEntry>& pending,
                                             Fn&&                             function)
        requires requires(const PendingEntry& entry, const Archetype* archetype) {
            { entry.filter.MatchArchetype(archetype) } -> std::same_as<bool>;
        }
    {
        componentManager.ForEachArchetype([&](Archetype* archetype) {
            std::vector<std::size_t> matchedIndexes;
            matchedIndexes.reserve(pending.size());

            for (std::size_t index = 0; index < pending.size(); ++index)
            {
                if (pending[index].filter.MatchArchetype(archetype))
                    matchedIndexes.push_back(index);
            }

            if (matchedIndexes.empty())
                return;

            function(archetype, matchedIndexes);
        });
    }

} // namespace FREYR_NAMESPACE

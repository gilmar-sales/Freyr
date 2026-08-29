#pragma once

#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/Filter.hpp"

namespace FREYR_NAMESPACE
{

    template <typename Fn>
    void ForEachMatchingArchetype(const ComponentManager& componentManager,
                                  const Filter&           filter,
                                  Fn&&                    function)
    {
        componentManager.ForEachArchetype([&](Archetype* archetype) {
            if (!filter.MatchArchetype(archetype))
                return;

            function(archetype);
        });
    }

} // namespace FREYR_NAMESPACE

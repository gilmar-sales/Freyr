#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"

namespace FREYR_NAMESPACE
{
    template <typename T>
    struct EntityRemapper
    {
        static constexpr bool kEnabled = false;

        template <typename Map>
        static void Remap(T&, Map&&)
        {
        }
    };

    template <>
    struct EntityRemapper<ChildOf>
    {
        static constexpr bool kEnabled = true;

        template <typename Map>
        static void Remap(ChildOf& childOf, Map&& map)
        {
            childOf.parent = map(childOf.parent);
        }
    };
} // namespace FREYR_NAMESPACE

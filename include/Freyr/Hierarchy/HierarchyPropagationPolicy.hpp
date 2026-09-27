#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"

#include <concepts>

namespace FREYR_NAMESPACE
{
    template <typename P>
    concept HierarchyPropagationPolicy = requires(P p, ComponentManager& cm, Entity parent, Entity child) {
        typename P::Local;
        typename P::World;
        requires IsHierarchyLocal<typename P::Local>;
        requires IsComponent<typename P::World>;
        { p.OnRoot(cm, child) } -> std::same_as<void>;
        { p.Propagate(cm, parent, child) } -> std::same_as<void>;
        { p.HasChildrenInterest(cm, child) } -> std::convertible_to<bool>;
    };
} // namespace FREYR_NAMESPACE

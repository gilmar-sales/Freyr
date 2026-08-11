#pragma once

#include "Freyr/Base/TypeNameId.hpp"

#include <Skirnir/Common/Reflection.hpp>

#include <cstdint>
#include <type_traits>

#ifndef FREYR_NAMESPACE
    #define FREYR_NAMESPACE fr
#endif

namespace FREYR_NAMESPACE
{

    /**
     * @brief Base type for all ECS components.
     *
     * Components must inherit from this struct to be recognized by the ECS framework.
     * They are data-only containers with no logic.
     *
     * @note Use the IsComponent concept to check if a type qualifies as a component.
     * @note Use GetComponentId<T>() to obtain a unique identifier for each component type.
     */
    using ComponentId = std::uint64_t;

    [[nodiscard]] inline auto ComponentCount() -> ComponentId
    {
        return TypeNameCount(TypeIdKind::Component);
    }

    /**
     * @brief Marker struct for components; inherit from this to define a component type.
     *
     * Components should be pure data containers (structs with only member variables).
     * Business logic belongs in Systems.
     */
    struct Component
    {
    };

    /**
     * @brief Concept that verifies if a type is a valid component.
     *
     * @tparam T  Type to check
     *
     * A type satisfies IsComponent if it inherits from Component.
     */
    template <typename T>
    concept IsComponent = std::is_base_of_v<Component, std::remove_reference_t<T>>;

    /**
     * @brief Returns a process-stable dense identifier for the given component type.
     *
     * @tparam T  Component type (must satisfy IsComponent)
     * @return ComponentId assigned from the process-global type-name registry
     *
     * @note Identity is keyed by refl::type_name<T>() so host, static libs, and plugins that
     *       share one Freyr copy observe the same id for the same type name.
     *       The function-local static only caches that lookup.
     */
    template <typename T>
        requires IsComponent<T>
    inline auto GetComponentId() -> ComponentId
    {
        static const auto id = RegisterTypeName(TypeIdKind::Component, refl::type_name<T>());
        return id;
    }
} // namespace FREYR_NAMESPACE

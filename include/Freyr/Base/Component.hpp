#pragma once

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
    using ComponentId                 = std::uint64_t;
    inline ComponentId ComponentCount = 0;

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
     * @brief Returns a unique identifier for the given component type.
     *
     * @tparam T  Component type (must satisfy IsComponent)
     * @return Unique ComponentId assigned at first call (static storage)
     *
     * @note IDs are assigned at runtime in declaration order across translation units.
     */
    template <typename T>
        requires IsComponent<T>
    inline constexpr auto GetComponentId() -> ComponentId
    {
        static auto id = ComponentCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE

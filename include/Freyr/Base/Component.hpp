#pragma once

#include "Freyr/Base/Event.hpp"
#include "Freyr/Base/TypeNameId.hpp"

#include <Skirnir/Common/Reflection.hpp>

#include <concepts>
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
     * Components are plain data structs with no logic.
     * They no longer need to inherit from a marker type; any movable,
     * default-constructible struct qualifies.
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
     * @brief Tag wrapper to request removal of T from an entity signature.
     *
     * Used in signature deltas (e.g. CreateOrUpdateEntityIndexWith) to express
     * "remove this component" alongside added components.
     */
    template <typename T>
    struct Remove
    {
    };

    template <typename T>
    struct is_remove : std::false_type
    {
    };
    template <typename T>
    struct is_remove<Remove<T>> : std::true_type
    {
    };

    template <typename T>
    struct unwrap_remove
    {
        using type = T;
    };
    template <typename T>
    struct unwrap_remove<Remove<T>>
    {
        using type = T;
    };
    template <typename T>
    using unwrap_remove_t = typename unwrap_remove<T>::type;

    /**
     * @brief Concept that verifies if a type is a valid component.
     *
     * @tparam T  Type to check
     *
     * A type satisfies IsComponent if it is an aggregate struct/class that is movable and
     * default-constructible, and is neither an Event nor a Remove<T> tag.
     * Aggregateness enforces data-only structs and excludes lambdas/callables.
     * Movability covers archetype migration and in-chunk reorder (swap-with-last);
     * default-constructibility covers chunk column storage (vector resize).
     */
    template <typename T>
    concept IsComponent = std::is_aggregate_v<std::remove_cvref_t<T>> &&
        std::movable<std::remove_cvref_t<T>> &&
        std::default_initializable<std::remove_cvref_t<T>> &&
        !IsEvent<std::remove_cvref_t<T>> && !is_remove<std::remove_cvref_t<T>>::value;

    /**
     * @brief Concept for components that can also be copied.
     *
     * @tparam T  Type to check
     *
     * Clone/CopyEntity paths require copyability; migration and reorder only
     * require movability (see IsComponent).
     */
    template <typename T>
    concept IsCopyableComponent = IsComponent<T> && std::copyable<std::remove_cvref_t<T>>;

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

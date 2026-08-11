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
     * @brief Base type and utilities for the event system.
     *
     * Events are lightweight notification payloads that listeners subscribe to.
     * Inherit from Event to define a new event type.
     */

    using EventId = std::uint64_t;

    [[nodiscard]] inline auto EventCount() -> EventId
    {
        return TypeNameCount(TypeIdKind::Event);
    }

    /**
     * @brief Marker struct for events; inherit from this to define an event type.
     *
     * Events should be simple data containers describing what happened.
     * Business logic belongs in listeners that receive events.
     */
    struct Event
    {
    };

    /**
     * @brief Concept that verifies if a type is a valid event.
     *
     * @tparam T  Type to check
     *
     * A type satisfies IsEvent if it inherits from Event.
     */
    template <typename T>
    concept IsEvent = std::is_base_of_v<Event, T>;

    /**
     * @brief Returns a process-stable dense identifier for the given event type.
     *
     * @tparam T  Event type (must satisfy IsEvent)
     * @return EventId assigned from the process-global type-name registry
     *
     * @note Identity is keyed by refl::type_name<T>() so host, static libs, and plugins that
     *       share one Freyr copy observe the same id for the same type name.
     *       The function-local static only caches that lookup.
     */
    template <typename T>
        requires IsEvent<T>
    inline auto GetEventId() -> EventId
    {
        static const auto id = RegisterTypeName(TypeIdKind::Event, refl::type_name<T>());
        return id;
    }
} // namespace FREYR_NAMESPACE

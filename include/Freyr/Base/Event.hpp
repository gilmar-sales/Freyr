#pragma once

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

    using EventId             = std::uint64_t;
    inline EventId EventCount = 0;

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
     * @brief Returns a unique identifier for the given event type.
     *
     * @tparam T  Event type (must satisfy IsEvent)
     * @return Unique EventId assigned at first call (static storage)
     *
     * @note IDs are assigned at runtime in declaration order across translation units.
     */
    template <typename T>
        requires IsEvent<T>
    constexpr auto GetEventId() -> EventId
    {
        static auto id = EventCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE

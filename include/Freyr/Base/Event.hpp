#pragma once

namespace FREYR_NAMESPACE
{
    using EventId = std::uint64_t;

    inline EventId EventCount = 0;

    struct Event
    {
    };

    template <typename T>
    concept IsEvent = std::is_base_of_v<Event, T>;

    template <typename T>
        requires IsEvent<T>
    constexpr auto GetEventId() -> EventId
    {
        const static auto id = EventCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE

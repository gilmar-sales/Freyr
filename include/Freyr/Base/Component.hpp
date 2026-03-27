#pragma once

namespace FREYR_NAMESPACE
{
    using ComponentId                              = std::uint64_t;
    inline std::atomic<ComponentId> ComponentCount = 0;

    struct Component
    {
    };

    template <typename T>
    concept IsComponent = std::is_base_of_v<Component, std::remove_reference_t<T>>;

    template <typename T>
        requires IsComponent<T>
    inline constexpr auto GetComponentId() -> ComponentId
    {
        static const auto id = ComponentCount.fetch_add(1, std::memory_order_relaxed);

        return id;
    }
} // namespace FREYR_NAMESPACE
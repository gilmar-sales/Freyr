#pragma once

namespace FREYR_NAMESPACE
{
    using ComponentId                 = std::uint64_t;
    inline ComponentId ComponentCount = 0;

    struct Component
    {
    };

    template <typename T>
    concept IsComponent =
        std::is_base_of_v<Component, std::remove_reference_t<T>>;

    template <typename T>
        requires IsComponent<T>
    constexpr auto GetComponentId() -> ComponentId
    {
        static auto id = ComponentCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE
#pragma once

namespace FREYR_NAMESPACE
{
    using ComponentId                 = std::uint64_t;
    inline ComponentId ComponentCount = 0;

    struct Component
    {
    };

    template <typename T>
    concept IsComponent = std::is_class_v<std::remove_reference_t<T>>;

    template <typename T>
        requires IsComponent<T>
    inline constexpr auto GetComponentId() -> ComponentId
    {
        static auto id = ComponentCount++;

        return id;
    }
} // namespace FREYR_NAMESPACE

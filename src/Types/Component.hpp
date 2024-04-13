#pragma once

#include "../Meta/Iteration.hpp"

namespace FREYR_NAMESPACE
{
    using ComponentId = std::uint64_t;

    inline ComponentId ComponentCount = 0;

    const ComponentId MAX_COMPONENTS = 1024;
    using Signature                  = std::bitset<MAX_COMPONENTS>;

    struct Component
    {
    };

    template<typename T>
    concept IsComponent = std::is_base_of_v<Component, T>;

    template<typename T>
        requires IsComponent<T>
    constexpr auto GetComponentId() -> ComponentId
    {
        const static auto id = ComponentCount++;

        return id;
    }

    template<typename... Components>
        requires(IsComponent<Components> and ...)
    constexpr auto GetSignature() -> Signature
    {
            auto signature = Signature{};
            meta::forEach([&signature](auto t) { signature.set(GetComponentId<decltype(t)>(), true); },
                          std::tuple<Components...>{});
        return signature;
    }

} // namespace FREYR_NAMESPACE
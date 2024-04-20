#pragma once

using DependencyId = std::uint64_t;

inline DependencyId DependencyCount = 0;

template <typename T>
constexpr auto GetDependencyId() -> DependencyId
{
    const static auto id = DependencyCount++;

    return id;
}
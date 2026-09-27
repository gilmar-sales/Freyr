#pragma once

#include <cstdint>
#include <limits>

namespace FREYR_NAMESPACE
{
    /**
     * @brief Entity identifier type.
     *
     * An Entity is just a 32-bit unsigned integer acting as a unique dense index.
     * Entities themselves have no data; data resides in Components attached to them.
     * For cross-entity references that survive recycle, store EntityHandle instead.
     */
    using Entity = std::uint32_t;

    using Generation = std::uint32_t;

    inline constexpr Entity NullEntity = std::numeric_limits<Entity>::max();

    struct EntityHandle
    {
        Entity     entity     = NullEntity;
        Generation generation = 0;

        [[nodiscard]] constexpr bool operator==(const EntityHandle&) const = default;
    };

    inline constexpr EntityHandle NullHandle {};
} // namespace FREYR_NAMESPACE

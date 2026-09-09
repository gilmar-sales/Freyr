#pragma once

#include <cstdint>
#include <limits>

namespace FREYR_NAMESPACE
{
    /**
     * @brief Entity identifier type.
     *
     * An Entity is just a 32-bit unsigned integer acting as a unique index.
     * Entities themselves have no data; data resides in Components attached to them.
     */
    using Entity = std::uint32_t;

    inline constexpr Entity NullEntity = std::numeric_limits<Entity>::max();
} // namespace FREYR_NAMESPACE

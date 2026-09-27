#pragma once

#include <cstdint>

namespace FREYR_NAMESPACE
{
    using Tick = std::uint32_t;

    struct ComponentTicks
    {
        Tick addedTick   = 0;
        Tick changedTick = 0;
    };
} // namespace FREYR_NAMESPACE

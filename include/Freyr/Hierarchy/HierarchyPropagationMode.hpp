#pragma once

#include <cstdint>

namespace FREYR_NAMESPACE
{
    enum class HierarchyPropagationMode : std::uint8_t
    {
        LevelSync,
        WorkSharing
    };
} // namespace FREYR_NAMESPACE

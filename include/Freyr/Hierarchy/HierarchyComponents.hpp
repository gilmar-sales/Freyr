#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"

#include <cstdint>

namespace FREYR_NAMESPACE
{
    struct ChildOf : Component
    {
        Entity parent = NullEntity;
    };

    struct ParentDepth : Component
    {
        std::uint16_t depth = 0;
    };
} // namespace FREYR_NAMESPACE

#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"

#include <cstdint>
#include <type_traits>

namespace FREYR_NAMESPACE
{
    struct HierarchyLocal : Component
    {
        bool isDirty = false;
    };

    template <typename T>
    concept IsHierarchyLocal = std::is_base_of_v<HierarchyLocal, std::remove_reference_t<T>>;

    struct ChildOf : Component
    {
        Entity parent = NullEntity;
    };

    struct ParentDepth : Component
    {
        std::uint16_t depth = 0;
    };
} // namespace FREYR_NAMESPACE

#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"

#include <cstdint>
#include <type_traits>

namespace FREYR_NAMESPACE
{
    struct HierarchyLocal
    {
        bool isDirty = false;
    };

    template <typename T>
    concept IsHierarchyLocal = std::is_base_of_v<HierarchyLocal, std::remove_reference_t<T>>;

    struct ChildOf
    {
        EntityHandle parent = NullHandle;
    };

    struct ParentDepth
    {
        std::uint16_t depth = 0;
    };
} // namespace FREYR_NAMESPACE

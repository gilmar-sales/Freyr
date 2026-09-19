#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/ArchetypeChunk.hpp"

#include <span>

namespace FREYR_NAMESPACE
{

    /**
     * @brief Read-only view over one archetype chunk's dense SoA columns.
     *
     * Columns and Entities() share the same index space: index i is the same entity
     * across every Column<T>() returned from this view.
     */
    class ChunkView
    {
      public:
        explicit ChunkView(const ArchetypeChunk& chunk) : mChunk(&chunk) {}

        [[nodiscard]] std::size_t size() const { return mChunk->Count(); }

        [[nodiscard]] bool empty() const { return size() == 0; }

        [[nodiscard]] std::span<const Entity> Entities() const { return mChunk->GetEntitiesSpan(); }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] std::span<const T> Column() const
        {
            return mChunk->GetComponentSpan<T>();
        }

      private:
        const ArchetypeChunk* mChunk;
    };

} // namespace FREYR_NAMESPACE

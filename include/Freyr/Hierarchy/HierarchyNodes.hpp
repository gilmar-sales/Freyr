#pragma once

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Containers/SparseSet.hpp"

#include <cstdint>
#include <vector>

namespace FREYR_NAMESPACE
{
    struct HierarchyNode
    {
        Entity        parent      = NullEntity;
        Entity        first       = NullEntity;
        Entity        last        = NullEntity;
        Entity        prev        = NullEntity;
        Entity        next        = NullEntity;
        std::uint16_t depth       = 0;
        std::uint8_t  dirty       = 0;
        std::uint8_t  syncPending = 0;
    };

    class DenseHierarchyNodes
    {
      public:
        explicit DenseHierarchyNodes(std::size_t capacity) : mNodes(capacity) {}

        [[nodiscard]] HierarchyNode*       Find(Entity entity) { return &mNodes[entity]; }
        [[nodiscard]] const HierarchyNode* Find(Entity entity) const { return &mNodes[entity]; }
        [[nodiscard]] HierarchyNode&       Get(Entity entity) { return mNodes[entity]; }
        [[nodiscard]] const HierarchyNode& Get(Entity entity) const { return mNodes[entity]; }
        HierarchyNode&                     Ensure(Entity entity) { return mNodes[entity]; }
        void                               Release(Entity entity) { mNodes[entity] = HierarchyNode {}; }

      private:
        std::vector<HierarchyNode> mNodes;
    };

    class SparseHierarchyNodes
    {
      public:
        using Index = LocalSparseSet<Entity>::Index;

        [[nodiscard]] HierarchyNode* Find(Entity entity)
        {
            const Index index = mIndex.find(entity);
            return index == LocalSparseSet<Entity>::npos ? nullptr : &mNodes[index];
        }

        [[nodiscard]] const HierarchyNode* Find(Entity entity) const
        {
            const Index index = mIndex.find(entity);
            return index == LocalSparseSet<Entity>::npos ? nullptr : &mNodes[index];
        }

        [[nodiscard]] HierarchyNode&       Get(Entity entity) { return mNodes[mIndex.index(entity)]; }
        [[nodiscard]] const HierarchyNode& Get(Entity entity) const
        {
            return mNodes[mIndex.index(entity)];
        }

        HierarchyNode& Ensure(Entity entity)
        {
            const Index index = mIndex.find(entity);
            if (index != LocalSparseSet<Entity>::npos)
                return mNodes[index];
            mIndex.insert(entity);
            return mNodes.emplace_back();
        }

        void Release(Entity entity)
        {
            const Index index = mIndex.find(entity);
            if (index == LocalSparseSet<Entity>::npos)
                return;
            mIndex.remove(entity);
            mNodes[index] = mNodes.back();
            mNodes.pop_back();
        }

        [[nodiscard]] std::size_t Size() const { return mNodes.size(); }

      private:
        LocalSparseSet<Entity>     mIndex;
        std::vector<HierarchyNode> mNodes;
    };
} // namespace FREYR_NAMESPACE

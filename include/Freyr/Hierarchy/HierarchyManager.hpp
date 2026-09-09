#pragma once

#include "Freyr/Containers/SparseSet.hpp"
#include "Freyr/Core/FreyrOptions.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationMode.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace FREYR_NAMESPACE
{
    class ComponentManager;

    class HierarchyManager
    {
      public:
        explicit HierarchyManager(const skr::Arc<FreyrOptions>& options);

        void BindComponentManager(const skr::Arc<ComponentManager>& componentManager);

        bool SetParent(Entity child, Entity parent);
        bool ClearParent(Entity child);

        [[nodiscard]] Entity GetParent(Entity child) const;
        [[nodiscard]] std::uint16_t GetDepth(Entity entity) const;
        [[nodiscard]] std::span<const Entity> Children(Entity parent) const;
        [[nodiscard]] std::uint16_t MaxDepth() const;
        [[nodiscard]] std::span<const Entity> EntitiesAtDepth(std::uint16_t depth) const;

        void EnsureDepthBuckets();
        void MarkDepthBucketsDirty() { mDepthBucketsDirty = true; }

        void FlushComponentSync();

        void MarkDirty(Entity entity);
        void ClearDirty();
        [[nodiscard]] bool IsDirty(Entity entity) const;
        [[nodiscard]] bool HasAnyDirty() const { return mAnyDirty; }

        void SetPropagationMode(HierarchyPropagationMode mode) { mPropagationMode = mode; }
        [[nodiscard]] HierarchyPropagationMode GetPropagationMode() const
        {
            return mPropagationMode;
        }

        void ExpandDestroySet(SparseSet<Entity>& toDestroy);
        void OnEntitiesDestroyed(const SparseSet<Entity>& destroyed);

        template <typename TFunc>
        void ForEachChild(Entity parent, TFunc&& func) const
        {
            for (const Entity child : Children(parent))
                func(child);
        }

        template <typename TFunc>
        void ForEachDescendant(Entity root, TFunc&& func) const
        {
            for (const Entity child : Children(root))
            {
                func(child);
                ForEachDescendant(child, func);
            }
        }

        template <typename TFunc>
        void ForEachParentWithChildren(TFunc&& func) const
        {
            for (const auto& [parent, kids] : mChildren)
            {
                if (!kids.empty())
                    func(parent);
            }
        }

        [[nodiscard]] bool HasChildren(Entity parent) const
        {
            return !Children(parent).empty();
        }

      private:
        [[nodiscard]] bool WouldCreateCycle(Entity child, Entity parent) const;
        void               DetachFromParent(Entity child);
        void               AttachToParent(Entity child, Entity parent);
        void               UpdateDepthRecursive(Entity entity, std::uint16_t depth);
        void               QueueComponentSync(Entity entity);
        void               SyncComponentsNow(Entity entity);
        void               RebuildDepthBuckets();
        void               RemoveFromDepthBucket(Entity entity, std::uint16_t depth);
        void               MarkDirtySubtree(Entity entity);

        skr::Arc<ComponentManager> mComponentManager;
        std::uint64_t              mMaxEntities;

        std::vector<Entity>        mParent;
        std::vector<std::uint16_t> mDepth;
        std::vector<std::uint32_t> mChildIndex;

        std::unordered_map<Entity, std::vector<Entity>> mChildren;
        std::vector<std::vector<Entity>>                mByDepth;
        bool                                            mDepthBucketsDirty = true;

        std::vector<char>   mDirty;
        bool                mAnyDirty = false;
        std::vector<char>   mSyncPending;
        std::vector<Entity> mSyncQueue;

        HierarchyPropagationMode mPropagationMode = HierarchyPropagationMode::WorkSharing;
    };
} // namespace FREYR_NAMESPACE

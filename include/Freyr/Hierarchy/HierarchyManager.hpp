#pragma once

#include "Freyr/Containers/SparseSet.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/EntityManager.hpp"
#include "Freyr/Core/FreyrOptions.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"
#include "Freyr/Hierarchy/HierarchyNodes.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationMode.hpp"
#include "Freyr/Hierarchy/HierarchyStorageMode.hpp"

#include <cstddef>
#include <iterator>
#include <span>
#include <vector>

namespace FREYR_NAMESPACE
{
    class HierarchyManager
    {
      public:
        class ChildRange
        {
          public:
            class Iterator
            {
              public:
                using iterator_category = std::forward_iterator_tag;
                using value_type        = Entity;
                using difference_type   = std::ptrdiff_t;
                using pointer           = const Entity*;
                using reference         = Entity;

                Iterator() = default;
                Iterator(const HierarchyManager* manager, Entity current) :
                    mManager(manager), mCurrent(current)
                {
                }

                Entity operator*() const { return mCurrent; }

                Iterator& operator++()
                {
                    mCurrent = mManager->GetNode(mCurrent).next;
                    return *this;
                }

                Iterator operator++(int)
                {
                    Iterator copy = *this;
                    ++*this;
                    return copy;
                }

                bool operator==(const Iterator& other) const { return mCurrent == other.mCurrent; }

              private:
                const HierarchyManager* mManager = nullptr;
                Entity                  mCurrent = NullEntity;
            };

            ChildRange() = default;
            ChildRange(const HierarchyManager* manager, Entity first) :
                mManager(manager), mFirst(first)
            {
            }

            [[nodiscard]] Iterator begin() const { return { mManager, mFirst }; }
            [[nodiscard]] Iterator end() const { return { mManager, NullEntity }; }
            [[nodiscard]] bool     empty() const { return mFirst == NullEntity; }
            [[nodiscard]] Entity   front() const { return mFirst; }

            [[nodiscard]] std::size_t size() const
            {
                return static_cast<std::size_t>(std::distance(begin(), end()));
            }

          private:
            const HierarchyManager* mManager = nullptr;
            Entity                  mFirst   = NullEntity;
        };

        explicit HierarchyManager(const skr::Arc<FreyrOptions>& options);

        void BindComponentManager(const skr::Arc<ComponentManager>& componentManager);
        void BindEntityManager(const skr::Arc<EntityManager>& entityManager);

        bool SetParent(Entity child, Entity parent);
        bool ClearParent(Entity child);

        [[nodiscard]] Entity GetParent(Entity child) const
        {
            const auto* node = FindNode(child);
            return node ? node->parent : NullEntity;
        }

        [[nodiscard]] std::uint16_t GetDepth(Entity entity) const
        {
            const auto* node = FindNode(entity);
            return node ? node->depth : 0;
        }

        [[nodiscard]] ChildRange Children(Entity parent) const
        {
            const auto* node = FindNode(parent);
            return { this, node ? node->first : NullEntity };
        }

        [[nodiscard]] bool HasChildren(Entity parent) const
        {
            const auto* node = FindNode(parent);
            return node && node->first != NullEntity;
        }

        [[nodiscard]] std::uint16_t           MaxDepth() const;
        [[nodiscard]] std::span<const Entity> EntitiesAtDepth(std::uint16_t depth) const;

        void EnsureDepthBuckets();
        void MarkDepthBucketsDirty() { mDepthBucketsDirty = true; }

        void FlushComponentSync();

        template <IsHierarchyLocal Local>
        void MarkDirty(Entity entity)
        {
            if (entity == NullEntity || entity >= mMaxEntities || !mComponentManager)
                return;
            if (const auto* node = FindNode(entity); node && node->dirty != 0)
                return;
            const bool marked = mComponentManager->TryGetComponents<Local>(
                entity, [](Local& local) { local.isDirty = true; });
            if (!marked)
                return;
            EnsureNode(entity).dirty = 1;
            mDirtyQueue.push_back(entity);
            mAnyDirty = true;
        }

        template <IsHierarchyLocal Local>
        [[nodiscard]] bool IsDirty(Entity entity) const
        {
            const auto* node = FindNode(entity);
            return node && node->dirty != 0;
        }

        void CollectDirtyHeads(std::vector<Entity>& heads);

        template <IsHierarchyLocal Local>
        void ClearDirty()
        {
            if (!mAnyDirty || !mComponentManager)
                return;
            for (const Entity entity : mDirtyQueue)
            {
                auto* node = FindNode(entity);
                if (!node || node->dirty == 0)
                    continue;
                node->dirty = 0;
                mComponentManager->TryGetComponents<Local>(
                    entity, [](Local& local) { local.isDirty = false; });
            }
            mDirtyQueue.clear();
            mAnyDirty = false;
        }

        [[nodiscard]] bool HasAnyDirty() const { return mAnyDirty; }

        void SetPropagationMode(HierarchyPropagationMode mode) { mPropagationMode = mode; }
        [[nodiscard]] HierarchyPropagationMode GetPropagationMode() const
        {
            return mPropagationMode;
        }

        [[nodiscard]] HierarchyStorageMode GetStorageMode() const { return mStorageMode; }

        void ExpandDestroySet(SparseSet<Entity>& toDestroy);
        void OnEntitiesDestroyed(const SparseSet<Entity>& destroyed);

        template <typename TFunc>
        void ForEachChild(Entity parent, TFunc&& func) const
        {
            if (mStorageMode == HierarchyStorageMode::Dense)
                ForEachChildIn(mDenseNodes, parent, func);
            else
                ForEachChildIn(mSparseNodes, parent, func);
        }

        template <typename TFunc>
        void ForEachDescendant(Entity root, TFunc&& func) const
        {
            ForEachChild(root, [&](Entity child) {
                func(child);
                ForEachDescendant(child, func);
            });
        }

        [[nodiscard]] std::span<const Entity> RootsWithChildren() const
        {
            return mRootsWithChildren.getDense();
        }

      private:
        template <typename TNodes, typename TFunc>
        void ForEachChildIn(const TNodes& nodes, Entity parent, TFunc& func) const
        {
            if (parent >= mMaxEntities)
                return;
            const auto* node = nodes.Find(parent);
            if (!node)
                return;
            for (Entity child = node->first; child != NullEntity;)
            {
                const Entity next = nodes.Get(child).next;
                func(child);
                child = next;
            }
        }

        [[nodiscard]] HierarchyNode* FindNode(Entity entity)
        {
            if (entity >= mMaxEntities)
                return nullptr;
            return mStorageMode == HierarchyStorageMode::Dense ? mDenseNodes.Find(entity)
                                                               : mSparseNodes.Find(entity);
        }

        [[nodiscard]] const HierarchyNode* FindNode(Entity entity) const
        {
            if (entity >= mMaxEntities)
                return nullptr;
            return mStorageMode == HierarchyStorageMode::Dense ? mDenseNodes.Find(entity)
                                                               : mSparseNodes.Find(entity);
        }

        [[nodiscard]] HierarchyNode& GetNode(Entity entity)
        {
            return mStorageMode == HierarchyStorageMode::Dense ? mDenseNodes.Get(entity)
                                                               : mSparseNodes.Get(entity);
        }

        [[nodiscard]] const HierarchyNode& GetNode(Entity entity) const
        {
            return mStorageMode == HierarchyStorageMode::Dense ? mDenseNodes.Get(entity)
                                                               : mSparseNodes.Get(entity);
        }

        HierarchyNode& EnsureNode(Entity entity)
        {
            return mStorageMode == HierarchyStorageMode::Dense ? mDenseNodes.Ensure(entity)
                                                               : mSparseNodes.Ensure(entity);
        }

        void ReleaseNode(Entity entity)
        {
            if (mStorageMode == HierarchyStorageMode::Dense)
                mDenseNodes.Release(entity);
            else
                mSparseNodes.Release(entity);
        }

        [[nodiscard]] bool WouldCreateCycle(Entity child, Entity parent) const;
        void               DetachFromParent(Entity child);
        void               AttachToParent(Entity child, Entity parent);
        void               UpdateDepthRecursive(Entity entity, std::uint16_t depth);
        void               QueueComponentSync(Entity entity, std::uint8_t flags);
        void               SyncComponentsNow(Entity entity, std::uint8_t flags);
        void               RebuildDepthBuckets();
        void               RefreshRootWithChildren(Entity entity);

        skr::Arc<ComponentManager> mComponentManager;
        skr::Arc<EntityManager>    mEntityManager;
        std::uint64_t              mMaxEntities;
        HierarchyStorageMode       mStorageMode;

        DenseHierarchyNodes  mDenseNodes;
        SparseHierarchyNodes mSparseNodes;

        LocalSparseSet<Entity> mRootsWithChildren;

        std::vector<std::vector<Entity>> mByDepth;
        bool                             mDepthBucketsDirty = true;

        bool                mAnyDirty = false;
        std::vector<Entity> mDirtyQueue;
        std::vector<Entity> mSyncQueue;

        HierarchyPropagationMode mPropagationMode = HierarchyPropagationMode::WorkSharing;
    };
} // namespace FREYR_NAMESPACE

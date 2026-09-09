#include "Freyr/Hierarchy/HierarchyManager.hpp"

#include "Freyr/Core/ComponentManager.hpp"

#include <algorithm>

namespace FREYR_NAMESPACE
{
    HierarchyManager::HierarchyManager(const skr::Arc<FreyrOptions>& options) :
        mMaxEntities(options->MaxEntities), mParent(options->MaxEntities, NullEntity),
        mDepth(options->MaxEntities, 0), mChildIndex(options->MaxEntities, 0),
        mDirty(options->MaxEntities, 0), mSyncPending(options->MaxEntities, 0)
    {
    }

    void HierarchyManager::BindComponentManager(const skr::Arc<ComponentManager>& componentManager)
    {
        mComponentManager = componentManager;
    }

    bool HierarchyManager::WouldCreateCycle(Entity child, Entity parent) const
    {
        if (parent == NullEntity)
            return false;

        Entity current = parent;
        while (current != NullEntity)
        {
            if (current == child)
                return true;
            current = mParent[current];
        }
        return false;
    }

    void HierarchyManager::DetachFromParent(Entity child)
    {
        const Entity parent = mParent[child];
        if (parent == NullEntity)
            return;

        auto it = mChildren.find(parent);
        if (it == mChildren.end())
        {
            mParent[child] = NullEntity;
            return;
        }

        auto&      kids  = it->second;
        const auto index = mChildIndex[child];
        if (index < kids.size() && kids[index] == child)
        {
            kids.erase(kids.begin() + static_cast<std::ptrdiff_t>(index));
            for (std::size_t i = index; i < kids.size(); ++i)
                mChildIndex[kids[i]] = static_cast<std::uint32_t>(i);
        }
        else
        {
            const auto found = std::find(kids.begin(), kids.end(), child);
            if (found != kids.end())
            {
                const auto eraseIndex = static_cast<std::size_t>(found - kids.begin());
                kids.erase(found);
                for (std::size_t i = eraseIndex; i < kids.size(); ++i)
                    mChildIndex[kids[i]] = static_cast<std::uint32_t>(i);
            }
        }

        if (kids.empty())
            mChildren.erase(it);

        mParent[child]     = NullEntity;
        mChildIndex[child] = 0;
    }

    void HierarchyManager::AttachToParent(Entity child, Entity parent)
    {
        auto& kids         = mChildren[parent];
        mChildIndex[child] = static_cast<std::uint32_t>(kids.size());
        kids.push_back(child);
        mParent[child] = parent;
    }

    void HierarchyManager::RemoveFromDepthBucket(Entity entity, std::uint16_t depth)
    {
        if (depth >= mByDepth.size())
            return;

        auto&      bucket = mByDepth[depth];
        const auto it     = std::find(bucket.begin(), bucket.end(), entity);
        if (it != bucket.end())
            bucket.erase(it);
    }

    void HierarchyManager::QueueComponentSync(Entity entity)
    {
        if (entity >= mMaxEntities || mSyncPending[entity])
            return;
        mSyncPending[entity] = 1;
        mSyncQueue.push_back(entity);
    }

    void HierarchyManager::SyncComponentsNow(Entity entity)
    {
        if (!mComponentManager)
            return;

        const Entity parent = mParent[entity];
        if (parent == NullEntity)
        {
            mComponentManager->RemoveComponentNow<ChildOf>(entity);
            mComponentManager->AddComponentNow(entity, ParentDepth {.depth = 0});
        }
        else
        {
            mComponentManager->AddComponentNow(entity, ChildOf {.parent = parent});
            mComponentManager->AddComponentNow(entity, ParentDepth {.depth = mDepth[entity]});
        }
    }

    void HierarchyManager::FlushComponentSync()
    {
        if (mSyncQueue.empty())
            return;

        for (const Entity entity : mSyncQueue)
        {
            if (entity >= mMaxEntities || !mSyncPending[entity])
                continue;
            mSyncPending[entity] = 0;
            SyncComponentsNow(entity);
        }
        mSyncQueue.clear();
    }

    void HierarchyManager::UpdateDepthRecursive(Entity entity, std::uint16_t depth)
    {
        mDepth[entity] = depth;
        QueueComponentSync(entity);

        for (const Entity child : Children(entity))
            UpdateDepthRecursive(child, static_cast<std::uint16_t>(depth + 1));
    }

    void HierarchyManager::MarkDirtySubtree(Entity entity)
    {
        if (entity >= mMaxEntities)
            return;
        mDirty[entity] = 1;
        for (const Entity child : Children(entity))
            MarkDirtySubtree(child);
    }

    void HierarchyManager::MarkDirty(Entity entity)
    {
        if (entity == NullEntity || entity >= mMaxEntities)
            return;
        MarkDirtySubtree(entity);
        Entity current = mParent[entity];
        while (current != NullEntity && current < mMaxEntities)
        {
            if (mDirty[current])
                break;
            mDirty[current] = 1;
            current         = mParent[current];
        }
        mAnyDirty = true;
    }

    void HierarchyManager::ClearDirty()
    {
        if (!mAnyDirty)
            return;
        std::fill(mDirty.begin(), mDirty.end(), 0);
        mAnyDirty = false;
    }

    bool HierarchyManager::IsDirty(Entity entity) const
    {
        if (entity == NullEntity || entity >= mMaxEntities)
            return false;
        return mDirty[entity] != 0;
    }

    bool HierarchyManager::SetParent(Entity child, Entity parent)
    {
        if (child == NullEntity || child >= mMaxEntities)
            return false;
        if (parent != NullEntity && parent >= mMaxEntities)
            return false;
        if (child == parent)
            return false;
        if (WouldCreateCycle(child, parent))
            return false;

        DetachFromParent(child);

        if (parent == NullEntity)
        {
            UpdateDepthRecursive(child, 0);
        }
        else
        {
            AttachToParent(child, parent);
            UpdateDepthRecursive(child, static_cast<std::uint16_t>(mDepth[parent] + 1));
        }

        mDepthBucketsDirty = true;
        return true;
    }

    bool HierarchyManager::ClearParent(Entity child)
    {
        return SetParent(child, NullEntity);
    }

    Entity HierarchyManager::GetParent(Entity child) const
    {
        if (child == NullEntity || child >= mMaxEntities)
            return NullEntity;
        return mParent[child];
    }

    std::uint16_t HierarchyManager::GetDepth(Entity entity) const
    {
        if (entity == NullEntity || entity >= mMaxEntities)
            return 0;
        return mDepth[entity];
    }

    std::span<const Entity> HierarchyManager::Children(Entity parent) const
    {
        const auto it = mChildren.find(parent);
        if (it == mChildren.end())
            return {};
        return it->second;
    }

    std::uint16_t HierarchyManager::MaxDepth() const
    {
        if (mByDepth.empty())
            return 0;
        return static_cast<std::uint16_t>(mByDepth.size() - 1);
    }

    std::span<const Entity> HierarchyManager::EntitiesAtDepth(std::uint16_t depth) const
    {
        if (depth >= mByDepth.size())
            return {};
        return mByDepth[depth];
    }

    void HierarchyManager::EnsureDepthBuckets()
    {
        if (mDepthBucketsDirty)
            RebuildDepthBuckets();
    }

    void HierarchyManager::RebuildDepthBuckets()
    {
        mByDepth.clear();

        for (const auto& [parent, kids] : mChildren)
        {
            (void) parent;
            for (const Entity child : kids)
            {
                const auto depth = mDepth[child];
                if (depth >= mByDepth.size())
                    mByDepth.resize(static_cast<std::size_t>(depth) + 1);
                mByDepth[depth].push_back(child);
            }
        }

        mDepthBucketsDirty = false;
    }

    void HierarchyManager::ExpandDestroySet(SparseSet<Entity>& toDestroy)
    {
        if (toDestroy.size() == 0)
            return;

        std::vector<Entity> seeds(toDestroy.begin(), toDestroy.end());
        std::vector<Entity> ordered;
        ordered.reserve(seeds.size());

        std::vector<char> visited(mMaxEntities, 0);

        const auto visit = [&](auto&& self, Entity entity) -> void {
            if (entity >= mMaxEntities || visited[entity])
                return;
            visited[entity] = 1;
            for (const Entity child : Children(entity))
                self(self, child);
            ordered.push_back(entity);
        };

        for (const Entity seed : seeds)
            visit(visit, seed);

        toDestroy.clear();
        for (const Entity entity : ordered)
            toDestroy.insert(entity);
    }

    void HierarchyManager::OnEntitiesDestroyed(const SparseSet<Entity>& destroyed)
    {
        for (const Entity entity : destroyed)
        {
            if (entity >= mMaxEntities)
                continue;

            for (const Entity child : Children(entity))
            {
                mParent[child]     = NullEntity;
                mChildIndex[child] = 0;
            }
            mChildren.erase(entity);

            DetachFromParent(entity);
            mParent[entity]     = NullEntity;
            mDepth[entity]      = 0;
            mChildIndex[entity] = 0;
            mDirty[entity]      = 0;
            mSyncPending[entity] = 0;
        }
        mDepthBucketsDirty = true;
    }
} // namespace FREYR_NAMESPACE

#include "Freyr/Hierarchy/HierarchyManager.hpp"

#include "Freyr/Core/ComponentManager.hpp"

namespace FREYR_NAMESPACE
{
    namespace
    {
        constexpr std::uint8_t kSyncParent = 1;
        constexpr std::uint8_t kSyncDepth  = 2;
    }

    HierarchyManager::HierarchyManager(const skr::Arc<FreyrOptions>& options) :
        mMaxEntities(options->MaxEntities), mStorageMode(options->HierarchyStorage),
        mDenseNodes(options->HierarchyStorage == HierarchyStorageMode::Dense ? options->MaxEntities
                                                                             : 0)
    {
    }

    void HierarchyManager::BindComponentManager(const skr::Arc<ComponentManager>& componentManager)
    {
        mComponentManager = componentManager;
    }

    void HierarchyManager::BindEntityManager(const skr::Arc<EntityManager>& entityManager)
    {
        mEntityManager = entityManager;
    }

    bool HierarchyManager::WouldCreateCycle(Entity child, Entity parent) const
    {
        Entity current = parent;
        while (current != NullEntity)
        {
            if (current == child)
                return true;
            current = GetParent(current);
        }
        return false;
    }

    void HierarchyManager::DetachFromParent(Entity child)
    {
        auto&        childNode = GetNode(child);
        const Entity parent    = childNode.parent;
        if (parent == NullEntity)
            return;

        auto& parentNode = GetNode(parent);
        if (childNode.prev != NullEntity)
            GetNode(childNode.prev).next = childNode.next;
        else
            parentNode.first = childNode.next;
        if (childNode.next != NullEntity)
            GetNode(childNode.next).prev = childNode.prev;
        else
            parentNode.last = childNode.prev;

        childNode.parent = NullEntity;
        childNode.prev   = NullEntity;
        childNode.next   = NullEntity;

        if (parentNode.first == NullEntity)
            mRootsWithChildren.remove(parent);
    }

    void HierarchyManager::AttachToParent(Entity child, Entity parent)
    {
        auto& childNode  = GetNode(child);
        auto& parentNode = GetNode(parent);

        childNode.parent = parent;
        childNode.prev   = parentNode.last;
        childNode.next   = NullEntity;
        if (parentNode.last != NullEntity)
            GetNode(parentNode.last).next = child;
        else
            parentNode.first = child;
        parentNode.last = child;

        if (parentNode.first == child)
            RefreshRootWithChildren(parent);
    }

    void HierarchyManager::RefreshRootWithChildren(Entity entity)
    {
        const auto& node = GetNode(entity);
        if (node.parent == NullEntity && node.first != NullEntity)
            mRootsWithChildren.insert(entity);
        else
            mRootsWithChildren.remove(entity);
    }

    void HierarchyManager::QueueComponentSync(Entity entity, std::uint8_t flags)
    {
        if (entity >= mMaxEntities)
            return;
        auto& node = EnsureNode(entity);
        if (node.syncPending == 0)
            mSyncQueue.push_back(entity);
        node.syncPending = static_cast<std::uint8_t>(node.syncPending | flags);
    }

    void HierarchyManager::SyncComponentsNow(Entity entity, std::uint8_t flags)
    {
        if (!mComponentManager)
            return;

        const auto&         node   = GetNode(entity);
        const Entity        parent = node.parent;
        const std::uint16_t depth  = node.depth;

        if ((flags & kSyncParent) != 0)
        {
            if (parent == NullEntity)
            {
                if (mComponentManager->HasComponent<ChildOf>(entity))
                    mComponentManager->RemoveComponentNow<ChildOf>(entity);
            }
            else
            {
                const EntityHandle parentHandle =
                    mEntityManager ? mEntityManager->HandleOf(parent)
                                   : EntityHandle {.entity = parent, .generation = 0};
                bool unchanged = false;
                mComponentManager->TryGetComponents<ChildOf>(entity, [&](ChildOf& childOf) {
                    unchanged = childOf.parent.entity == parentHandle.entity &&
                                childOf.parent.generation == parentHandle.generation;
                });
                if (!unchanged)
                    mComponentManager->SetComponentNow(entity, ChildOf {.parent = parentHandle});
            }
        }

        bool depthUnchanged = false;
        mComponentManager->TryGetComponents<ParentDepth>(entity, [&](ParentDepth& current) {
            depthUnchanged = current.depth == depth;
        });
        if (!depthUnchanged)
            mComponentManager->SetComponentNow(entity, ParentDepth {.depth = depth});
    }

    void HierarchyManager::FlushComponentSync()
    {
        if (mSyncQueue.empty())
            return;

        for (const Entity entity : mSyncQueue)
        {
            auto* node = FindNode(entity);
            if (!node || node->syncPending == 0)
                continue;
            const auto flags  = node->syncPending;
            node->syncPending = 0;
            SyncComponentsNow(entity, flags);
        }
        mSyncQueue.clear();
    }

    void HierarchyManager::UpdateDepthRecursive(Entity entity, std::uint16_t depth)
    {
        auto& node = GetNode(entity);
        if (node.depth == depth)
            return;
        node.depth = depth;
        QueueComponentSync(entity, kSyncDepth);

        ForEachChild(entity, [&](Entity child) {
            UpdateDepthRecursive(child, static_cast<std::uint16_t>(depth + 1));
        });
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

        EnsureNode(child);
        if (parent != NullEntity)
            EnsureNode(parent);

        DetachFromParent(child);
        QueueComponentSync(child, kSyncParent);

        if (parent == NullEntity)
        {
            UpdateDepthRecursive(child, 0);
        }
        else
        {
            AttachToParent(child, parent);
            UpdateDepthRecursive(child, static_cast<std::uint16_t>(GetNode(parent).depth + 1));
        }
        RefreshRootWithChildren(child);

        mDepthBucketsDirty = true;
        return true;
    }

    bool HierarchyManager::ClearParent(Entity child)
    {
        return SetParent(child, NullEntity);
    }

    void HierarchyManager::CollectDirtyHeads(std::vector<Entity>& heads)
    {
        for (const Entity entity : mDirtyQueue)
        {
            auto* node = FindNode(entity);
            if (!node || node->dirty != 1)
                continue;

            bool   covered = false;
            Entity current = node->parent;
            while (current != NullEntity)
            {
                const auto& ancestor = GetNode(current);
                if (ancestor.dirty != 0)
                {
                    covered = true;
                    break;
                }
                current = ancestor.parent;
            }

            node->dirty = 2;
            if (!covered)
                heads.push_back(entity);
        }
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

        std::vector<Entity> stack;
        for (const Entity root : mRootsWithChildren)
            ForEachChild(root, [&](Entity child) { stack.push_back(child); });

        while (!stack.empty())
        {
            const Entity entity = stack.back();
            stack.pop_back();

            const auto depth = GetNode(entity).depth;
            if (depth >= mByDepth.size())
                mByDepth.resize(static_cast<std::size_t>(depth) + 1);
            mByDepth[depth].push_back(entity);

            ForEachChild(entity, [&](Entity child) { stack.push_back(child); });
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
            ForEachChild(entity, [&](Entity child) { self(self, child); });
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
            auto* node = FindNode(entity);
            if (!node)
                continue;

            for (Entity child = node->first; child != NullEntity;)
            {
                auto&        childNode = GetNode(child);
                const Entity next      = childNode.next;
                childNode.parent       = NullEntity;
                childNode.prev         = NullEntity;
                childNode.next         = NullEntity;
                RefreshRootWithChildren(child);
                child = next;
            }
            node->first = NullEntity;
            node->last  = NullEntity;

            DetachFromParent(entity);
            mRootsWithChildren.remove(entity);
            ReleaseNode(entity);
        }
        mDepthBucketsDirty = true;
    }
} // namespace FREYR_NAMESPACE

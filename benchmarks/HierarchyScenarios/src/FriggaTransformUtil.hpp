#pragma once

#include <Freyr/Freyr.hpp>
#include <Freyr/Hierarchy/Policies/Transform3DPolicy.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <unordered_set>
#include <vector>

namespace frigga
{
    inline constexpr fr::Entity kInvalidEntity = static_cast<fr::Entity>(-1);

    using TransformComponent = fr::Transform3D;
    using Pose               = fr::Transform3D;
    using Mat4               = std::array<float, 16>;

    struct HierarchyComponent : fr::Component
    {
        fr::Entity              parent = kInvalidEntity;
        std::vector<fr::Entity> children;
    };

    struct NameComponent : fr::Component
    {
        char value[32] {};
    };

    namespace TransformUtil
    {
        inline constexpr Mat4 kIdentity { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                          0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f };

        inline Mat4 LocalMatrix(const TransformComponent& transform)
        {
            Mat4 matrix;
            fr::ComposeMatrix(transform, matrix.data());
            return matrix;
        }

        inline fr::Entity ParentOf(fr::Registry& registry, fr::Entity entity)
        {
            fr::Entity parent = kInvalidEntity;
            if (entity == kInvalidEntity)
                return parent;
            registry.TryGetComponents<HierarchyComponent>(
                entity, [&](HierarchyComponent& hierarchy) { parent = hierarchy.parent; });
            if (parent == kInvalidEntity)
                return kInvalidEntity;
            if (!registry.HasComponent<HierarchyComponent>(parent) &&
                !registry.HasComponent<TransformComponent>(parent) &&
                !registry.HasComponent<NameComponent>(parent))
                return kInvalidEntity;
            return parent;
        }

        inline bool WouldCreateCycle(fr::Registry& registry, fr::Entity entity,
                                     fr::Entity newParent)
        {
            if (entity == kInvalidEntity || newParent == kInvalidEntity)
                return false;
            if (entity == newParent)
                return true;
            std::unordered_set<fr::Entity> seen;
            auto                           current = newParent;
            while (current != kInvalidEntity)
            {
                if (current == entity)
                    return true;
                if (!seen.insert(current).second)
                    return true;
                current = ParentOf(registry, current);
            }
            return false;
        }

        inline Mat4 WorldMatrix(fr::Registry& registry, fr::Entity entity)
        {
            constexpr std::size_t                      kMaxHierarchyDepth = 64;
            std::array<fr::Entity, kMaxHierarchyDepth> chain {};
            std::size_t                                count = 0;

            auto current = entity;
            while (current != kInvalidEntity && count < kMaxHierarchyDepth)
            {
                if (std::find(chain.begin(), chain.begin() + count, current) !=
                    chain.begin() + count)
                    break;
                chain[count++] = current;
                current        = ParentOf(registry, current);
            }

            Mat4 world = kIdentity;
            for (std::size_t i = count; i > 0; --i)
            {
                registry.TryGetComponents<TransformComponent>(
                    chain[i - 1], [&](TransformComponent& transform) {
                        const auto local = LocalMatrix(transform);
                        fr::MultiplyMat4(world.data(), local.data(), world.data());
                    });
            }
            return world;
        }

        inline Pose WorldPose(fr::Registry& registry, fr::Entity entity)
        {
            const auto world = WorldMatrix(registry, entity);
            return fr::DecomposeMatrix(world.data());
        }

        inline Mat4 ParentWorldMatrix(fr::Registry& registry, fr::Entity entity)
        {
            const auto parent = ParentOf(registry, entity);
            if (parent == kInvalidEntity)
                return kIdentity;
            return WorldMatrix(registry, parent);
        }

        inline void SetWorldMatrix(fr::Registry& registry, fr::Entity entity, const Mat4& world)
        {
            if (!registry.HasComponent<TransformComponent>(entity))
                return;
            const Mat4 parentWorld = ParentWorldMatrix(registry, entity);
            Mat4       inverse;
            fr::InverseAffineMatrix(parentWorld.data(), inverse.data());
            Mat4 local;
            fr::MultiplyMat4(inverse.data(), world.data(), local.data());
            registry.TryGetComponents<TransformComponent>(
                entity,
                [&](TransformComponent& transform) {
                    transform = fr::DecomposeMatrix(local.data());
                });
        }

        inline void SetWorldPose(fr::Registry& registry, fr::Entity entity,
                                 const float (&position)[3], const float (&rotation)[4])
        {
            const auto current = WorldPose(registry, entity);
            Pose       target  = current;
            for (int i = 0; i < 3; ++i)
                target.position[i] = position[i];
            for (int i = 0; i < 4; ++i)
                target.rotation[i] = rotation[i];
            Mat4 world;
            fr::ComposeMatrix(target, world.data());
            SetWorldMatrix(registry, entity, world);
        }

        inline void EnsureHierarchy(fr::Registry& registry, fr::Entity entity)
        {
            if (entity == kInvalidEntity)
                return;
            if (!registry.HasComponent<HierarchyComponent>(entity))
            {
                registry.AddComponents(entity, HierarchyComponent {});
                registry.ExecuteTasks();
            }
        }

        inline void DetachFromParent(fr::Registry& registry, fr::Entity entity)
        {
            const auto oldParent = ParentOf(registry, entity);
            if (oldParent == kInvalidEntity)
                return;
            registry.TryGetComponents<HierarchyComponent>(
                oldParent,
                [&](HierarchyComponent& hierarchy) { std::erase(hierarchy.children, entity); });
        }

        inline bool SetParent(fr::Registry& registry, fr::Entity entity, fr::Entity newParent,
                              bool preserveWorld = true)
        {
            if (entity == kInvalidEntity || entity == newParent)
                return false;
            if (newParent != kInvalidEntity && WouldCreateCycle(registry, entity, newParent))
                return false;

            Mat4       world = kIdentity;
            const bool captureWorld =
                preserveWorld && registry.HasComponent<TransformComponent>(entity);
            if (captureWorld)
                world = WorldMatrix(registry, entity);

            DetachFromParent(registry, entity);
            EnsureHierarchy(registry, entity);
            registry.TryGetComponents<HierarchyComponent>(
                entity, [&](HierarchyComponent& hierarchy) { hierarchy.parent = newParent; });

            if (newParent != kInvalidEntity)
            {
                EnsureHierarchy(registry, newParent);
                registry.TryGetComponents<HierarchyComponent>(
                    newParent, [&](HierarchyComponent& hierarchy) {
                        if (std::ranges::find(hierarchy.children, entity) ==
                            hierarchy.children.end())
                            hierarchy.children.push_back(entity);
                    });
            }

            if (captureWorld)
                SetWorldMatrix(registry, entity, world);
            return true;
        }

        inline void CollectSubtree(fr::Registry& registry, fr::Entity entity,
                                   std::vector<fr::Entity>&        out,
                                   std::unordered_set<fr::Entity>& seen)
        {
            if (entity == kInvalidEntity || !seen.insert(entity).second)
                return;
            std::vector<fr::Entity> children;
            registry.TryGetComponents<HierarchyComponent>(
                entity, [&](HierarchyComponent& hierarchy) { children = hierarchy.children; });
            for (const auto child : children)
                CollectSubtree(registry, child, out, seen);
            out.push_back(entity);
        }

        inline void DestroySubtree(fr::Registry& registry, fr::Entity entity)
        {
            std::vector<fr::Entity>        order;
            std::unordered_set<fr::Entity> seen;
            CollectSubtree(registry, entity, order, seen);
            for (const auto node : order)
            {
                DetachFromParent(registry, node);
                registry.DestroyEntity(node);
            }
            registry.ExecuteTasks();
        }
    } // namespace TransformUtil
} // namespace frigga

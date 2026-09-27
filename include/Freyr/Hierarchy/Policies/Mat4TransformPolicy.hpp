#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationPolicy.hpp"

namespace FREYR_NAMESPACE
{
    struct LocalTransform3D : HierarchyLocal
    {
        float matrix[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                            0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    };

    struct WorldTransform3D : Component
    {
        float matrix[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                            0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    };

    inline void MultiplyMat4(const float* a, const float* b, float* out)
    {
        float result[16];
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                result[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0] +
                                        a[1 * 4 + row] * b[col * 4 + 1] +
                                        a[2 * 4 + row] * b[col * 4 + 2] +
                                        a[3 * 4 + row] * b[col * 4 + 3];
            }
        }
        for (int i = 0; i < 16; ++i)
            out[i] = result[i];
    }

    inline LocalTransform3D TranslationLocal3D(float x, float y, float z)
    {
        LocalTransform3D t {};
        t.matrix[12] = x;
        t.matrix[13] = y;
        t.matrix[14] = z;
        return t;
    }

    struct Mat4TransformPolicy
    {
        using Local = LocalTransform3D;
        using World = WorldTransform3D;

        void OnRoot(ComponentManager& cm, Entity entity) const
        {
            const auto& local = cm.GetComponent<Local>(entity);
            auto&       world = cm.GetComponent<World>(entity);
            for (int i = 0; i < 16; ++i)
                world.matrix[i] = local.matrix[i];
        }

        void Propagate(ComponentManager& cm, Entity parent, Entity child) const
        {
            const auto& parentWorld = cm.GetComponent<World>(parent);
            const auto& local       = cm.GetComponent<Local>(child);
            auto&       world       = cm.GetComponent<World>(child);
            MultiplyMat4(parentWorld.matrix, local.matrix, world.matrix);
        }

        bool HasChildrenInterest(ComponentManager&, Entity) const { return true; }
    };
} // namespace FREYR_NAMESPACE

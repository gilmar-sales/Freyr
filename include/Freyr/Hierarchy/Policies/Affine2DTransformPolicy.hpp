#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationPolicy.hpp"

#include <cmath>

namespace FREYR_NAMESPACE
{
    struct LocalTransform2D : HierarchyLocal
    {
        float x        = 0.f;
        float y        = 0.f;
        float rotation = 0.f;
        float scaleX   = 1.f;
        float scaleY   = 1.f;
    };

    struct WorldTransform2D : Component
    {
        float m00 = 1.f, m01 = 0.f, m02 = 0.f;
        float m10 = 0.f, m11 = 1.f, m12 = 0.f;
    };

    inline WorldTransform2D LocalToWorld2D(const LocalTransform2D& local)
    {
        const float c = std::cos(local.rotation);
        const float s = std::sin(local.rotation);
        WorldTransform2D world {};
        world.m00 = c * local.scaleX;
        world.m01 = -s * local.scaleY;
        world.m02 = local.x;
        world.m10 = s * local.scaleX;
        world.m11 = c * local.scaleY;
        world.m12 = local.y;
        return world;
    }

    inline WorldTransform2D MultiplyAffine2D(const WorldTransform2D& a, const WorldTransform2D& b)
    {
        WorldTransform2D out {};
        out.m00 = a.m00 * b.m00 + a.m01 * b.m10;
        out.m01 = a.m00 * b.m01 + a.m01 * b.m11;
        out.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02;
        out.m10 = a.m10 * b.m00 + a.m11 * b.m10;
        out.m11 = a.m10 * b.m01 + a.m11 * b.m11;
        out.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12;
        return out;
    }

    struct Affine2DTransformPolicy
    {
        using Local = LocalTransform2D;
        using World = WorldTransform2D;

        void OnRoot(ComponentManager& cm, Entity entity) const
        {
            cm.GetComponent<World>(entity) = LocalToWorld2D(cm.GetComponent<Local>(entity));
        }

        void Propagate(ComponentManager& cm, Entity parent, Entity child) const
        {
            const auto childLocal = LocalToWorld2D(cm.GetComponent<Local>(child));
            cm.GetComponent<World>(child) =
                MultiplyAffine2D(cm.GetComponent<World>(parent), childLocal);
        }

        bool HasChildrenInterest(ComponentManager&, Entity) const { return true; }
    };
} // namespace FREYR_NAMESPACE

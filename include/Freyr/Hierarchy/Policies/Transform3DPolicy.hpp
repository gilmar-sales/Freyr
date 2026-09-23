#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationPolicy.hpp"
#include "Freyr/Hierarchy/Policies/Mat4TransformPolicy.hpp"

#include <cmath>

namespace FREYR_NAMESPACE
{
    struct Transform3D : HierarchyLocal
    {
        float position[3] = { 0.f, 0.f, 0.f };
        float rotation[4] = { 0.f, 0.f, 0.f, 1.f };
        float scale[3]    = { 1.f, 1.f, 1.f };
    };

    inline void ComposeMatrix(const Transform3D& transform, float* out)
    {
        const float x = transform.rotation[0], y = transform.rotation[1], z = transform.rotation[2],
                    w  = transform.rotation[3];
        const float xx = x * x, yy = y * y, zz = z * z;
        const float xy = x * y, xz = x * z, yz = y * z;
        const float wx = w * x, wy = w * y, wz = w * z;
        const float sx = transform.scale[0], sy = transform.scale[1], sz = transform.scale[2];

        out[0]  = (1.f - 2.f * (yy + zz)) * sx;
        out[1]  = 2.f * (xy + wz) * sx;
        out[2]  = 2.f * (xz - wy) * sx;
        out[3]  = 0.f;
        out[4]  = 2.f * (xy - wz) * sy;
        out[5]  = (1.f - 2.f * (xx + zz)) * sy;
        out[6]  = 2.f * (yz + wx) * sy;
        out[7]  = 0.f;
        out[8]  = 2.f * (xz + wy) * sz;
        out[9]  = 2.f * (yz - wx) * sz;
        out[10] = (1.f - 2.f * (xx + yy)) * sz;
        out[11] = 0.f;
        out[12] = transform.position[0];
        out[13] = transform.position[1];
        out[14] = transform.position[2];
        out[15] = 1.f;
    }

    inline bool InverseAffineMatrix(const float* m, float* out)
    {
        const float a00 = m[0], a10 = m[1], a20 = m[2];
        const float a01 = m[4], a11 = m[5], a21 = m[6];
        const float a02 = m[8], a12 = m[9], a22 = m[10];

        const float c00 = a11 * a22 - a12 * a21;
        const float c01 = a12 * a20 - a10 * a22;
        const float c02 = a10 * a21 - a11 * a20;
        const float det = a00 * c00 + a01 * c01 + a02 * c02;
        if (std::fabs(det) < 1e-12f)
            return false;
        const float inv = 1.f / det;

        const float i00 = c00 * inv;
        const float i01 = (a02 * a21 - a01 * a22) * inv;
        const float i02 = (a01 * a12 - a02 * a11) * inv;
        const float i10 = c01 * inv;
        const float i11 = (a00 * a22 - a02 * a20) * inv;
        const float i12 = (a02 * a10 - a00 * a12) * inv;
        const float i20 = c02 * inv;
        const float i21 = (a01 * a20 - a00 * a21) * inv;
        const float i22 = (a00 * a11 - a01 * a10) * inv;

        const float tx = m[12], ty = m[13], tz = m[14];

        out[0]  = i00;
        out[1]  = i10;
        out[2]  = i20;
        out[3]  = 0.f;
        out[4]  = i01;
        out[5]  = i11;
        out[6]  = i21;
        out[7]  = 0.f;
        out[8]  = i02;
        out[9]  = i12;
        out[10] = i22;
        out[11] = 0.f;
        out[12] = -(i00 * tx + i01 * ty + i02 * tz);
        out[13] = -(i10 * tx + i11 * ty + i12 * tz);
        out[14] = -(i20 * tx + i21 * ty + i22 * tz);
        out[15] = 1.f;
        return true;
    }

    inline Transform3D DecomposeMatrix(const float* m)
    {
        Transform3D transform {};
        transform.position[0] = m[12];
        transform.position[1] = m[13];
        transform.position[2] = m[14];

        float cols[3][3] = {
            { m[0], m[1], m[2] },
            { m[4], m[5], m[6] },
            { m[8], m[9], m[10] },
        };
        for (int c = 0; c < 3; ++c)
        {
            const float length = std::sqrt(
                cols[c][0] * cols[c][0] + cols[c][1] * cols[c][1] + cols[c][2] * cols[c][2]);
            transform.scale[c] = length;
            if (length > 1e-8f)
            {
                for (int r = 0; r < 3; ++r)
                    cols[c][r] /= length;
            }
            else
            {
                for (int r = 0; r < 3; ++r)
                    cols[c][r] = r == c ? 1.f : 0.f;
            }
        }

        const float det = cols[0][0] * (cols[1][1] * cols[2][2] - cols[2][1] * cols[1][2]) -
                          cols[1][0] * (cols[0][1] * cols[2][2] - cols[2][1] * cols[0][2]) +
                          cols[2][0] * (cols[0][1] * cols[1][2] - cols[1][1] * cols[0][2]);
        if (det < 0.f)
        {
            transform.scale[0] = -transform.scale[0];
            for (int r = 0; r < 3; ++r)
                cols[0][r] = -cols[0][r];
        }

        const float m00 = cols[0][0], m11 = cols[1][1], m22 = cols[2][2];
        const float trace = m00 + m11 + m22;
        float       q[4];
        if (trace > 0.f)
        {
            const float s = std::sqrt(trace + 1.f) * 2.f;
            q[3]          = 0.25f * s;
            q[0]          = (cols[1][2] - cols[2][1]) / s;
            q[1]          = (cols[2][0] - cols[0][2]) / s;
            q[2]          = (cols[0][1] - cols[1][0]) / s;
        }
        else if (m00 > m11 && m00 > m22)
        {
            const float s = std::sqrt(1.f + m00 - m11 - m22) * 2.f;
            q[3]          = (cols[1][2] - cols[2][1]) / s;
            q[0]          = 0.25f * s;
            q[1]          = (cols[1][0] + cols[0][1]) / s;
            q[2]          = (cols[2][0] + cols[0][2]) / s;
        }
        else if (m11 > m22)
        {
            const float s = std::sqrt(1.f + m11 - m00 - m22) * 2.f;
            q[3]          = (cols[2][0] - cols[0][2]) / s;
            q[0]          = (cols[1][0] + cols[0][1]) / s;
            q[1]          = 0.25f * s;
            q[2]          = (cols[2][1] + cols[1][2]) / s;
        }
        else
        {
            const float s = std::sqrt(1.f + m22 - m00 - m11) * 2.f;
            q[3]          = (cols[0][1] - cols[1][0]) / s;
            q[0]          = (cols[2][0] + cols[0][2]) / s;
            q[1]          = (cols[2][1] + cols[1][2]) / s;
            q[2]          = 0.25f * s;
        }

        const float norm = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        for (int i = 0; i < 4; ++i)
            transform.rotation[i] = q[i] / norm;
        return transform;
    }

    inline Transform3D LocalFromWorld(const float* parentWorld, const float* world)
    {
        float inverseParent[16];
        if (!InverseAffineMatrix(parentWorld, inverseParent))
            return DecomposeMatrix(world);
        float local[16];
        MultiplyMat4(inverseParent, world, local);
        return DecomposeMatrix(local);
    }

    struct Transform3DPolicy
    {
        using Local = Transform3D;
        using World = WorldTransform3D;

        void OnRoot(ComponentManager& cm, Entity entity) const
        {
            ComposeMatrix(cm.GetComponent<Local>(entity), cm.GetComponent<World>(entity).matrix);
        }

        void Propagate(ComponentManager& cm, Entity parent, Entity child) const
        {
            float local[16];
            ComposeMatrix(cm.GetComponent<Local>(child), local);
            MultiplyMat4(cm.GetComponent<World>(parent).matrix, local,
                         cm.GetComponent<World>(child).matrix);
        }

        bool HasChildrenInterest(ComponentManager&, Entity) const { return true; }
    };
} // namespace FREYR_NAMESPACE

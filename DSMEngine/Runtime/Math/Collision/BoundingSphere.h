#pragma once
#ifndef __BOUNDING_SPHERE_H__
#define __BOUNDING_SPHERE_H__


#include "BoundingBox.h"

namespace DSM::Math {
    class BoundingSphere
    {
    public:
        BoundingSphere() = default;
        BoundingSphere(const Vector3& center, Scalar radius)
            : m_Sphere(center, std::max(float(radius), 0.0f)) {}
        BoundingSphere(const Vector4& sphere)
            : m_Sphere(sphere) {}
        BoundingSphere(float x, float y, float z, float radius)
            : m_Sphere({x, y, z, std::max(radius, 0.0f)}) {}
        BoundingSphere(const AxisAlignedBox& boundingBox)
        {
            Vector3 center = boundingBox.GetCenter();
            Vector3 size = boundingBox.GetSize() * 0.5f;
            float radius = size.Magnitude();
            if (size.Get(0) < 0.f || size.Get(1) < 0.f || size.Get(2) < 0.f) {
                radius = 0;
            }
            m_Sphere = Vector4(center, radius);
        }

        Vector3 GetCenter() const noexcept { return Vector3{ m_Sphere }; }
        Scalar GetRadius() const noexcept { return m_Sphere.Get(3); }

        BoundingSphere& operator*=(const Transform& transform) noexcept
        {
            auto center = GetCenter() + transform.GetPosition();
            float radius = GetRadius();
            auto scale = transform.GetScale();
            float scaleMax = std::max({scale.Get(0), scale.Get(1), scale.Get(2)});
            radius *= scaleMax;
            m_Sphere = Vector4(center, radius);
            return *this;
        }

        static BoundingSphere Union(BoundingSphere lfs, const BoundingSphere& rhs) noexcept
        {
            float radl = lfs.GetRadius();
            float radr = rhs.GetRadius();
            Vector3 dir = rhs.GetCenter() - lfs.GetCenter();

            float rad = (radl + radr + dir.Magnitude()) * 0.5f;
            Vector3 center = lfs.GetCenter() + dir.Normalized() * (rad - radl);
            return BoundingSphere(center, rad);
        }

    private:
        Vector4 m_Sphere{}; // (x, y, z, radius)
    };

    inline BoundingSphere operator*(BoundingSphere sphere, const Transform& transform) noexcept { return sphere *= transform; }
} // namespace DSM::Math



#endif // __BOUNDING_SPHERE_H__
#pragma once
#ifndef __BOUNDING_SPHERE_H__
#define __BOUNDING_SPHERE_H__

#include "Runtime/Math/MathCommon.h"

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

        Vector3 GetCenter() const noexcept { return Vector3{ m_Sphere }; }
        Scalar GetRadius() const noexcept { return m_Sphere.Get(3); }

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
} // namespace DSM::Math



#endif // __BOUNDING_SPHERE_H__
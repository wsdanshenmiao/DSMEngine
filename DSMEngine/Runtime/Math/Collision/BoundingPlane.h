#pragma once
#ifndef __BOUNDING_PLANE_H__
#define __BOUNDING_PLANE_H__

#include "Runtime/Math/Transform.h"

namespace DSM::Math {
    // 定义一个平面，法线沿着顶点朝向平面做垂线
    class BoundingPlane
    {
    public:
        BoundingPlane() noexcept = default;
        // normal 为平面的法线，沿着远点到平面，d 为平面到原点的距离
        BoundingPlane(const Vector3& normal, float d) noexcept
            : m_Plane(normal.Normalized(), d) {}
        BoundingPlane(const Vector3& normal, const Vector3& pointOnPlane) noexcept
            : m_Plane(normal.Normalized(), -Vector3::Dot(normal.Normalized(), pointOnPlane)) {}
        BoundingPlane(float a, float b, float c, float d) noexcept
            : m_Plane({a, b, c, d}) {}
        explicit BoundingPlane(const Vector4& plane) noexcept
            : m_Plane(plane) {}

        bool operator==(const BoundingPlane& other) const noexcept = default;
        friend BoundingPlane operator*(const BoundingPlane& plane, const Matrix4& matrix) noexcept;
        friend BoundingPlane operator*(const BoundingPlane& plane, const Transform& transform) noexcept;

        Vector3 GetNormal() const noexcept { return Vector3{m_Plane}; }
        Vector3 GetPointOnPlane() const noexcept { return m_Plane.Get(3) * -GetNormal(); }

        Scalar GetDistanceFromPoint(const Vector3& point) const noexcept
        {
            return Vector3::Dot(GetNormal(), point) + m_Plane.Get(3);
        }
        Scalar GetDistanceFromPoint(const Vector4& point) const noexcept
        {
            return Vector4::Dot(m_Plane, point);
        }

    private:
        Vector4 m_Plane{};    // (normal, d)
    };

    inline BoundingPlane operator*(const BoundingPlane& plane, const Transform& transform) noexcept
    {
        Vector3 normal = transform.GetRotation() * plane.GetNormal();
        float d = plane.m_Plane.Get(3) - Vector3::Dot(normal, transform.GetPosition());
        return BoundingPlane{normal, d};
    }

    inline BoundingPlane operator*(const BoundingPlane& plane, const Matrix4& matrix) noexcept
    {
        Vector3 normal = Quaternion{matrix} * plane.GetNormal();
        float d = plane.m_Plane.Get(3) - Vector3::Dot(normal, Vector3{matrix.Get(3)});
        return BoundingPlane{normal, d};
    }
} // namespace DSM::Math



#endif // !__BOUNDING_PLANE_H__
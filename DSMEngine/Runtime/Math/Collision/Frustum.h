#pragma once
#ifndef __FRUSTUM_H__
#define __FRUSTUM_H__

#include "Runtime/Math/Collision/BoundingBox.h"
#include "Runtime/Math/Collision/BoundingPlane.h"
#include "Runtime/Math/Collision/BoundingSphere.h"

namespace DSM::Math {
    class Frustum 
    {
    public:
        enum CornerID
        {
            NearTopLeft = 0,
            NearTopRight = 1,
            NearBottomRight = 2,
            NearBottomLeft = 3,
            FarTopLeft = 4,
            FarTopRight = 5,
            FarBottomRight = 6,
            FarBottomLeft = 7,
            CornerCount = 8
        };

        enum PlaneID
        {
            NearPlane = 0,
            FarPlane = 1,
            RightPlane = 2,
            LeftPlane = 3,
            TopPlane = 4,
            BottomPlane = 5,
            PlaneCount = 6
        };

    public:
        Frustum() noexcept
            : m_Origin({0.0f, 0.0f, 0.0f})
            , m_Orientation()
            , m_RightSlope(1.0f)
            , m_LeftSlope(-1.0f)
            , m_TopSlope(1.0f)
            , m_BottomSlope(-1.0f)
            , m_NearPlane(0.0f)
            , m_FarPlane(1.0f) {}
        Frustum(const Matrix4& projection) noexcept
        {
            // Corners of the projection frustum in NDC space.
            static Vector4 NDCPoints[6] = {
                {  1.0f,  0.0f, 1.0f, 1.0f },   // right (at far plane)
                { -1.0f,  0.0f, 1.0f, 1.0f },   // left
                {  0.0f,  1.0f, 1.0f, 1.0f },   // top
                {  0.0f, -1.0f, 1.0f, 1.0f },   // bottom

                { 0.0f, 0.0f, 0.0f, 1.0f },     // near
                { 0.0f, 0.0f, 1.0f, 1.0f }      // far
            };

            Matrix4 matInverse = Matrix4::Inverse(projection);

            // Compute the frustum corners in world space.
            Vector4 Points[6];

            for (size_t i = 0; i < 6; ++i)
            {
                // Transform point.
                Points[i] = NDCPoints[i] * matInverse;
            }

            m_Origin = Vector3{0.0f, 0.0f, 0.0f};
            m_Orientation = Quaternion{};

            // Compute the slopes.
            Points[0] = Points[0] / Vector4(Points[0].Get(2));
            Points[1] = Points[1] / Vector4(Points[1].Get(2));
            Points[2] = Points[2] / Vector4(Points[2].Get(2));
            Points[3] = Points[3] / Vector4(Points[3].Get(2));

            m_RightSlope = Points[0].Get(0);
            m_LeftSlope = Points[1].Get(0);
            m_TopSlope = Points[2].Get(1);
            m_BottomSlope = Points[3].Get(1);

            // Compute near and far.
            Points[4] = Points[4] / Vector4(Points[4].Get(3));
            Points[5] = Points[5] / Vector4(Points[5].Get(3));

            m_NearPlane = Points[4].Get(2);
            m_FarPlane = Points[5].Get(2);
        }
        Frustum(const Vector4& origin, const Quaternion& orientation,
            float rightSlope, float leftSlope,
            float topSlope, float bottomSlope,
            float nearPlane, float farPlane) noexcept
            : m_Origin(origin)
            , m_Orientation(orientation)
            , m_RightSlope(rightSlope)
            , m_LeftSlope(leftSlope)
            , m_TopSlope(topSlope)
            , m_BottomSlope(bottomSlope)
            , m_NearPlane(nearPlane)
            , m_FarPlane(farPlane) {}

        Vector3 GetCorner( CornerID id ) const
        {
            Vector3 corner{};
            switch (id) {
            case NearTopLeft:
                corner = Vector3{ m_LeftSlope, m_TopSlope, 1 } * m_NearPlane; break;
            case NearTopRight:
                corner = Vector3{ m_RightSlope, m_TopSlope, 1 } * m_NearPlane; break;
            case NearBottomLeft:
                corner = Vector3{ m_RightSlope, m_BottomSlope, 1 } * m_NearPlane; break;
            case NearBottomRight:
                corner = Vector3{ m_LeftSlope, m_BottomSlope, 1 } * m_NearPlane; break;
            case FarTopLeft:
                corner = Vector3{ m_LeftSlope, m_TopSlope, 1 } * m_FarPlane; break;
            case FarTopRight:
                corner = Vector3{ m_RightSlope, m_TopSlope, 1 } * m_FarPlane; break;
            case FarBottomLeft:
                corner = Vector3{ m_RightSlope, m_BottomSlope, 1 } * m_FarPlane; break;
            case FarBottomRight:
                corner = Vector3{ m_LeftSlope, m_BottomSlope, 1 } * m_FarPlane; break;
            default:
                assert(!"Invalid frustum corner ID");
                break;
            }

            // 先旋转后平移
            corner = m_Orientation * corner;
            return corner + m_Origin;
        }

        BoundingPlane GetPlane( PlaneID id ) const
        {
            Transform transform{Transform{m_Origin, {1,1,1,1}, m_Orientation}};
            switch (id) {
            case NearPlane:
                return BoundingPlane{Vector3{0.0f, 0.0f, 1.0f}, -m_NearPlane} * transform; break;
            case FarPlane:
                return BoundingPlane{Vector3{0.0f, 0.0f, -1.0f}, m_FarPlane} * transform; break;
            case RightPlane:
                return BoundingPlane{Vector3{-1.0f, 0.0f, m_RightSlope}, 0.0f} * transform; break;
            case LeftPlane:
                return BoundingPlane{Vector3{1.0f, 0.0f, -m_LeftSlope}, 0.0f} * transform; break;
            case TopPlane:
                return BoundingPlane{Vector3{0.0f, -1.0f, m_TopSlope}, 0.0f} * transform; break;
            case BottomPlane:
                return BoundingPlane{Vector3{0.0f, 1.0f, -m_BottomSlope}, 0.0f} * transform; break;
            default:
                assert(!"Invalid frustum plane ID");
                break;
            }

            return BoundingPlane{};
        }


        bool Intersects(const AxisAlignedBox& box) const noexcept
        {
            for(int i = 0; i < PlaneCount; i++) {
                auto plane = GetPlane(static_cast<PlaneID>(i));
                Vector3 boxMin = box.GetMin();
                Vector3 boxMax = box.GetMax();
                Vector3 normal = plane.GetNormal();
                // 获取离平面最近的点
                Vector3 nearCorner{
                    normal.Get(0) < 0.f ? boxMin.Get(0) : boxMax.Get(0),
                    normal.Get(1) < 0.f ? boxMin.Get(1) : boxMax.Get(1),
                    normal.Get(2) < 0.f ? boxMin.Get(2) : boxMax.Get(2)
                };
                if(plane.GetDistanceFromPoint(nearCorner) < 0.f)
                    return false;
            }
            return true;
        }

        bool Intersects(const OrientedBox& box) const noexcept
        {
            Vector3 boxSize = box.GetSize() * 0.5f;
            for(int i = 0; i < PlaneCount; i++) {
                int outCount = 0;
                auto plane = GetPlane(static_cast<PlaneID>(i));
                // 检测八个顶点
                for(int j = 0; j < CornerID::CornerCount; j++) {
                    Vector3 dir = boxSize * Vector3{
                        (j & 1) ? 1.f : -1.f,
                        (j & 2) ? 1.f : -1.f,
                        (j & 4) ? 1.f : -1.f
                    };
                    Vector3 corner = box.GetCenter() + box.GetOrientation() * dir;
                    if(plane.GetDistanceFromPoint(corner) < 0.f)
                        ++outCount;
                }
                if(outCount == CornerID::CornerCount)
                    return false;
            }
            return true;
        }

        bool Intersects(const BoundingSphere& sphere) const noexcept
        {
            float radius = sphere.GetRadius();
            for(int i = 0; i < PlaneCount; i++) {
                auto plane = GetPlane(static_cast<PlaneID>(i));
                if(plane.GetDistanceFromPoint(sphere.GetCenter()) + radius < 0)
                    return false;
            }
            return true;
        }

        Frustum& operator*=(const Transform& transform) noexcept
        {
            m_Origin += transform.GetPosition();
            m_Orientation = m_Orientation * transform.GetRotation();
            Vector3 scale = transform.GetScale();
            float scaleF = std::max(scale.Get(0), std::max(scale.Get(1), scale.Get(2)));
            m_NearPlane *= scaleF;
            m_FarPlane *= scaleF;
            return *this;
        }



    private:
        Vector3 m_Origin;
        Quaternion m_Orientation;

        float m_RightSlope;
        float m_LeftSlope;
        float m_TopSlope;
        float m_BottomSlope;
        float m_NearPlane;
        float m_FarPlane;
    };

    inline Frustum operator*(Frustum frustum, const Transform& transform) noexcept { return frustum *= transform; }
} // namespace DSM::Math


#endif // !__FRUSTUM_H__
#pragma once
#ifndef __BOUNDING_BOX_H__
#define __BOUNDING_BOX_H__

#include <span>
#include "Runtime/Framework/Component/Transform.h"

namespace DSM::Math {
    class AxisAlignedBox
    {
    public:
        AxisAlignedBox()
            : m_Min(std::numeric_limits<float>::max()), m_Max(std::numeric_limits<float>::lowest()) {}
        AxisAlignedBox(const Vector3& min, const Vector3& max)
        {
            m_Min = Vector3::Min(min, max);
            m_Max = Vector3::Max(min, max);
            PadToMinimums();
        }
        AxisAlignedBox(std::span<Vector3> points)
        {
            if(points.empty()){
                m_Min = Vector3(std::numeric_limits<float>::max());
                m_Max = Vector3(std::numeric_limits<float>::lowest());
            }
            else{
                m_Min = m_Max = points[0];
                for (size_t i = 1; i < points.size(); i++)
                {
                    m_Min = Vector3::Min(m_Min, points[i]);
                    m_Max = Vector3::Max(m_Max, points[i]);
                }
                PadToMinimums();
            }
        }

        Vector2 operator[](size_t index) const
        {
            return Vector2{m_Min.Get(index), m_Max.Get(index)};
        }

        bool IsValid() const noexcept { return Math::Vector3::Min(m_Min, m_Max) == m_Min; }

        bool Hit(const Vector3& origin, const Vector3& dir, const Vector2& tRange, Vector2& time) const
        {
            float tMin = tRange.Get(0);
            float tMax = tRange.Get(1);
            for (int axis = 0; axis < 3; axis++) {
                const Vector2& ax = Vector2{m_Min.Get(axis), m_Max.Get(axis)};
                const float adinv = 1.0f / dir.Get(axis);

                auto t0 = (ax.Get(0) - origin.Get(axis)) * adinv;
                auto t1 = (ax.Get(1) - origin.Get(axis)) * adinv;
                if(t0 > t1) 
                    std::swap(t0, t1);

                tMin = std::max(tMin, t0);
                tMax = std::min(tMax, t1);

                if (tMax <= tMin)
                    return false;
            }
            time = Vector2{tMin, tMax};
            return true;
        }

        size_t GetLongestAxis() const
        {
            size_t index = 0;
            for(size_t i = 1; i < 3; i++) {
                if(m_Max.Get(i) - m_Min.Get(i) > m_Max.Get(index) - m_Min.Get(index)) {
                    index = i;
                }
            }
            return index;
        }

        inline const Vector3& GetMin() const noexcept { return m_Min; }
        inline const Vector3& GetMax() const noexcept { return m_Max; }
        inline Vector3 GetCenter() const noexcept { return (m_Min + m_Max) * 0.5f; }
        inline Vector3 GetSize() const noexcept { return m_Max - m_Min; }

        float Area() const noexcept
        {
            Vector3 size = GetSize();
            return (size.Get(0) * size.Get(1) + size.Get(1) * size.Get(2) + size.Get(2) * size.Get(0)) * 2.f;
        }

        bool Contains(const AxisAlignedBox& other) const noexcept
        {
            return Vector3::Max(m_Max, other.m_Max) == m_Max && Vector3::Min(m_Min, other.m_Min) == m_Min;
        }

        static AxisAlignedBox Union(AxisAlignedBox a, const AxisAlignedBox& b) noexcept
        {
            a.m_Min = Vector3::Min(a.m_Min, b.m_Min);
            a.m_Max = Vector3::Max(a.m_Max, b.m_Max);
            a.PadToMinimums();
            return a;
        }

        AxisAlignedBox& operator*=(const Transform& transform) noexcept
        {
            auto center = GetCenter();
            auto extents = GetSize() * 0.5f;
            Matrix4 world = transform.GetLocalToWorld();
            Matrix3 absWorld{
                Vector3::Abs(Vector3{world.Get(0)}),
                Vector3::Abs(Vector3{world.Get(1)}),
                Vector3::Abs(Vector3{world.Get(2)})
            };
            center = Vector3{Vector4{center, 1.0f} * world};
            extents = extents * absWorld;
            m_Min = center - extents;
            m_Max = center + extents;
            return *this;
        }

    private:
        void PadToMinimums()
        {
            const float padding = 1e-4f;
            for(int i = 0; i < 3; i++) {
                if(m_Max.Get(i) - m_Min.Get(i) < padding) {
                    m_Max.Set(i, m_Min.Get(i) + padding);
                }
            }
        }

    private:
        Vector3 m_Min;
        Vector3 m_Max;
    };

    inline AxisAlignedBox operator*(AxisAlignedBox box, const Transform& transform) noexcept { return box *= transform; }


    class OrientedBox
    {
    public:
        OrientedBox(const AxisAlignedBox& box, const Quaternion& orientation)
            : m_Box(box), m_Orientation(orientation) {}
        
        inline Quaternion GetOrientation() const noexcept { return m_Orientation; }

        inline Vector3 GetCenter() const noexcept { return m_Box.GetCenter(); }
        inline Vector3 GetSize() const noexcept { return m_Box.GetSize(); }

    private:
        AxisAlignedBox m_Box;
        Quaternion m_Orientation;
    };

}


#endif
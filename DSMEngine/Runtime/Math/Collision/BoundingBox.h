#pragma once
#ifndef __BOUNDING_BOX_H__
#define __BOUNDING_BOX_H__

#include "Runtime/Math/MathCommon.h"

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

        Vector2 GetLongestAxis() const noexcept
        {
            int index = 0;
            for(int i = 1; i < 3; i++) {
                if(m_Max.Get(i) - m_Min.Get(i) > m_Max.Get(index) - m_Min.Get(index)) {
                    index = i;
                }
            }
            return Vector2{m_Min.Get(index), m_Max.Get(index)};
        }

        inline const Vector3& GetMin() const noexcept { return m_Min; }
        inline const Vector3& GetMax() const noexcept { return m_Max; }
        inline const Vector3& GetCenter() const noexcept { return (m_Min + m_Max) * 0.5f; }
        inline const Vector3& GetSize() const noexcept { return m_Max - m_Min; }

        static AxisAlignedBox Union(AxisAlignedBox a, const AxisAlignedBox& b) noexcept
        {
            a.m_Min = Vector3::Min(a.m_Min, b.m_Min);
            a.m_Max = Vector3::Max(a.m_Max, b.m_Max);
            a.PadToMinimums();
            return a;
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
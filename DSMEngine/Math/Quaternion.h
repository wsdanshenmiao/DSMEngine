#pragma once
#ifndef __QUATERNION_H__
#define __QUATERNION_H__

#include <cmath>
#include <stdexcept>
#include "Vector.h"

namespace DSM {
    class Quaternion {
    public:
        Quaternion() noexcept : m_Vector({1,0,0,0}) {}
        Quaternion(const Vector3f& axis, float angle) noexcept 
        {
            float s = std::sin(angle/2);
            float c = std::cos(angle/2);
            m_Vector.Set(0, c);                     // w
            m_Vector.Set(1, axis.Get(0) * s);       // x
            m_Vector.Set(2, axis.Get(1) * s);        // y
            m_Vector.Set(3, axis.Get(2) * s);       // z
        }
        Quaternion(float pitch, float yaw, float roll) noexcept 
        {
            float cr = std::cos(roll/2), sr = std::sin(roll/2);
            float cp = std::cos(pitch/2), sp = std::sin(pitch/2);
            float cy = std::cos(yaw/2), sy = std::sin(yaw/2);
            m_Vector.Set(0, cr*cp*cy + sr*sp*sy);
            m_Vector.Set(1, sr*cp*cy - cr*sp*sy);
            m_Vector.Set(2, cr*sp*cy + sr*cp*sy);
            m_Vector.Set(3, cr*cp*sy - sr*sp*cy);
        }

        Quaternion operator-() const { return Quaternion(Vector4f{-m_Vector.Get(0),-m_Vector.Get(1),-m_Vector.Get(2),-m_Vector.Get(3)}); }
        Quaternion operator~() const { return Quaternion(Vector4f{m_Vector.Get(0),-m_Vector.Get(1),-m_Vector.Get(2),-m_Vector.Get(3)}); }
        Quaternion& operator*=(const Quaternion& other) noexcept 
        {
            auto a = m_Vector;
            auto b = other.m_Vector;
            m_Vector.Set(0, a.Get(0)*b.Get(0) - a.Get(1)*b.Get(1) - a.Get(2)*b.Get(2) - a.Get(3)*b.Get(3));
            m_Vector.Set(1, a.Get(0)*b.Get(1) + a.Get(1)*b.Get(0) + a.Get(2)*b.Get(3) - a.Get(3)*b.Get(2));
            m_Vector.Set(2, a.Get(0)*b.Get(2) - a.Get(1)*b.Get(3) + a.Get(2)*b.Get(0) + a.Get(3)*b.Get(1));
            m_Vector.Set(3, a.Get(0)*b.Get(3) + a.Get(1)*b.Get(2) - a.Get(2)*b.Get(1) + a.Get(3)*b.Get(0));
            return *this;
        }

        inline bool operator==(const Quaternion& o) { return m_Vector == o.m_Vector; } 

        inline Scalarf Get(size_t index) const noexcept { return m_Vector.Get(index); }
        inline void Set(size_t index, float val) noexcept { m_Vector.Set(index, val); }

        static Quaternion Normalize(Quaternion q) 
        {
            auto v = q.m_Vector;
            float mag = std::sqrt(v.SqrMagnitude());
            if(mag > 1e-6f) v /= mag;
            return Quaternion(v);
        }

        static Quaternion Slerp(Quaternion q0, Quaternion q1, float t) 
        {
            // 先计算夹角
            float dot = DSM::Vector<float,4>::Dot(q0.m_Vector,q1.m_Vector);
            if(dot < 0) { q1.m_Vector *= -1; dot = -dot; }
            if(dot > 0.9995f) return Lerp(q0,q1,t);

            float theta_0 = std::acos(dot);
            float theta = theta_0 * t;
            DSM::Vector<float,4> v2 = q1.m_Vector - q0.m_Vector*dot;
            v2 = v2.Normalized();
            DSM::Vector<float,4> result = q0.m_Vector*std::cos(theta) + v2*std::sin(theta);
            return Quaternion(result);
        }

        static Quaternion Lerp(Quaternion q0, Quaternion q1, float t) 
        {
            DSM::Vector<float,4> v = q0.m_Vector*(1-t) + q1.m_Vector*t;
            return Normalize(Quaternion(v));
        }

    private:
        explicit Quaternion(const DSM::Vector<float,4>& v) : m_Vector(v) {}

        Vector<float,4> m_Vector; // w,x,y,z
    };

    inline Quaternion operator*(Quaternion q0, Quaternion q1)  { return q0 *= q1; }
    inline Vector3f operator*(const Quaternion& q, const Vector3f& vec) 
    {
        Quaternion qv(Vector3f{vec.Get(0), vec.Get(1), vec.Get(2)}, 0.0f); // w=0
        Quaternion res = q * qv * ~q;
        return Vector3f{res.Get(1),res.Get(2),res.Get(3)};
    }


} // namespace DSM 




#endif
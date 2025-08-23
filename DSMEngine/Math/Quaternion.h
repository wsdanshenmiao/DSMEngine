#pragma once
#ifndef __QUATERNION_H__
#define __QUATERNION_H__

#include <cmath>
#include <stdexcept>
#include <numbers>
#include "Vector.h"

namespace DSM {
    template<typename T> requires std::is_arithmetic_v<T>
    class Quaternion {
    public:
        Quaternion() noexcept : m_Vector({1,0,0,0}) {}
        Quaternion(Vector<T, 3> axis, T angle) noexcept 
        {
            Vector<T, 3>::Normalize(axis);
            T s = std::sin(angle * T(0.5));
            T c = std::cos(angle * T(0.5));
            m_Vector.Set(0, c);                     // w
            m_Vector.Set(1, axis.Get(0) * s);       // x
            m_Vector.Set(2, axis.Get(1) * s);        // y
            m_Vector.Set(3, axis.Get(2) * s);       // z
        }
        Quaternion(T pitch, T yaw, T roll) noexcept 
        {
            T cr = std::cos(roll * T(0.5)), sr = std::sin(roll * T(0.5));
            T cp = std::cos(pitch * T(0.5)), sp = std::sin(pitch * T(0.5));
            T cy = std::cos(yaw * T(0.5)), sy = std::sin(yaw * T(0.5));
            m_Vector.Set(0, cr*cp*cy + sr*sp*sy);
            m_Vector.Set(1, sr*cp*cy - cr*sp*sy);
            m_Vector.Set(2, cr*sp*cy + sr*cp*sy);
            m_Vector.Set(3, cr*cp*sy - sr*sp*cy);
        }

        Quaternion operator-() const { return Quaternion(Vector<T, 4>{-m_Vector.Get(0),-m_Vector.Get(1),-m_Vector.Get(2),-m_Vector.Get(3)}); }
        Quaternion operator~() const { return Quaternion(Vector<T, 4>{m_Vector.Get(0),-m_Vector.Get(1),-m_Vector.Get(2),-m_Vector.Get(3)}); }
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

        inline Scalar<T> Get(size_t index) const noexcept { return m_Vector.Get(index); }
        inline void Set(size_t index, T val) noexcept { m_Vector.Set(index, val); }

        inline Quaternion Normalized() const noexcept { Quaternion ret = *this; Normalize(ret); return ret; }

        static void Normalize(Quaternion& q) noexcept
        {
            auto& v = q.m_Vector;
            T mag = std::sqrt(v.SqrMagnitude());
            if(mag > 1e-6f) v /= mag;
        }

        static Quaternion Slerp(Quaternion q0, Quaternion q1, T t) 
        {
            // 先计算夹角
            T dot = Vector<T,4>::Dot(q0.m_Vector,q1.m_Vector);
            if(dot < 0) { q1.m_Vector *= -1; dot = -dot; }
            if(dot > 0.9995f) return Lerp(q0,q1,t);

            T theta_0 = std::acos(dot);
            T theta = theta_0 * t;
            Vector<T,4> v2 = q1.m_Vector - q0.m_Vector*dot;
            v2 = v2.Normalized();
            Vector<T,4> result = q0.m_Vector*std::cos(theta) + v2*std::sin(theta);
            return Quaternion(result);
        }

        static Quaternion Lerp(Quaternion q0, Quaternion q1, T t) 
        {
            Vector<T,4> v = q0.m_Vector * (1 - t) + q1.m_Vector * t;
            q0 = Quaternion{v};
            Normalize(q0);
            return q0;
        }

    private:
        explicit Quaternion(const DSM::Vector<T,4>& v) : m_Vector(v) {}

        Vector<T,4> m_Vector; // w,x,y,z
    };

    template<typename T> requires std::is_arithmetic_v<T>
    inline Quaternion<T> operator*(Quaternion<T> q0, Quaternion<T> q1)  { return q0 *= q1; }
    template<typename T> requires std::is_arithmetic_v<T>
    inline Vector<T, 3> operator*(Quaternion<T> q, const Vector<T, 3>& vec) 
    {
        Quaternion<T>::Normalize(q);
        Quaternion<T> qv{};
        qv.Set(0, 0);
        qv.Set(1, vec.Get(0));
        qv.Set(2, vec.Get(1));
        qv.Set(3, vec.Get(2));
        Quaternion<T> res = q * qv * (~q);
        return Vector<T, 3>{res.Get(1),res.Get(2),res.Get(3)};
    }

    using Quaternionf = Quaternion<float>;
    using Quaterniond = Quaternion<double>;
    using Quaternioni = Quaternion<int>;

} // namespace DSM 


template<typename T>
struct std::formatter<DSM::Quaternion<T>>
{
    template<typename Context>
    constexpr auto parse(Context& ctx) { return ctx.begin(); }

    template<typename Context>
    auto format(const DSM::Quaternion<T>& k, Context& ctx) const 
    {
        return std::format_to(ctx.out(), "{}, {}, {}, {}/n", k.Get(0), k.Get(1), k.Get(2), k.Get(3));
    }
};


#endif
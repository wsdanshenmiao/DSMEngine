#pragma once
#ifndef __QUATERNION_H__
#define __QUATERNION_H__

#include <cmath>
#include <stdexcept>
#include <numbers>
#include "Vector.h"

namespace DSM {
	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	class Matrix;

    template<typename T> requires std::is_arithmetic_v<T>
    class Quaternion {
    public:
        Quaternion() noexcept : m_Vector({0,0,0,1}) {}
        Quaternion(Vector<T, 3> axis, T angle) noexcept 
        {
            Vector<T, 3>::Normalize(axis);
            T s = std::sin(angle * T(0.5));
            T c = std::cos(angle * T(0.5));
            m_Vector.Set(0, axis.Get(0) * s);       // x
            m_Vector.Set(1, axis.Get(1) * s);       // y
            m_Vector.Set(2, axis.Get(2) * s);       // z
            m_Vector.Set(3, c);                     // w
        }
        Quaternion(T pitch, T yaw, T roll) noexcept : Quaternion(Vector<T, 3>{pitch, yaw, roll}) {}
        Quaternion(const Vector<T, 3>& v) noexcept
        {
            T pi2 = T(std::numbers::pi) * T(2);
            T pitch = v.Get(0) % pi2, yaw = v.Get(1) % pi2, roll = v.Get(2) % pi2;
            T cr = std::cos(roll * T(0.5)), sr = std::sin(roll * T(0.5));
            T cp = std::cos(pitch * T(0.5)), sp = std::sin(pitch * T(0.5));
            T cy = std::cos(yaw * T(0.5)), sy = std::sin(yaw * T(0.5));
            m_Vector.Set(0, sr*cp*cy - cr*sp*sy); // x
            m_Vector.Set(1, cr*sp*cy + sr*cp*sy); // y
            m_Vector.Set(2, cr*cp*sy - sr*sp*cy); // z
            m_Vector.Set(3, cr*cp*cy + sr*sp*sy); // w
        }
        Quaternion(T x, T y, T z, T w) noexcept : m_Vector({x, y, z, w}) {}
        explicit Quaternion(const Matrix<T, 3, 3>& m) noexcept : Quaternion(Matrix<T, 4, 4>(m)) {}
        explicit Quaternion(const Matrix<T, 4, 4>& m) noexcept
        {
            T x, y, z, w;
            T m00 = m.Get(0, 0);
            T m01 = m.Get(0, 1);
            T m02 = m.Get(0, 2);
            T m10 = m.Get(1, 0);
            T m11 = m.Get(1, 1);
            T m12 = m.Get(1, 2);
            T m20 = m.Get(2, 0);
            T m21 = m.Get(2, 1);
            T m22 = m.Get(2, 2);
            // 计算迹
            const T epsilon = T(1e-6);
            T trace = m00 + m11 + m22;
            if (trace > epsilon) {
                T s = T(0.5) / std::sqrt(trace + 1);
                w = T(0.25) / s;
                x = (m21 - m12) * s;
                y = (m02 - m20) * s;
                z = (m10 - m01) * s;
            } 
            else {
                if (m00 > m11 && m00 > m22) {
                    T s = T(0.5) / std::sqrt(1 + m00 - m11 - m22);
                    x = T(0.25) / s;
                    y = (m01 + m10) * s;
                    z = (m02 + m20) * s;
                    w = (m21 - m12) * s;
                } 
                else if (m11 > m22) {
                    T s = T(0.5) * std::sqrt(1 + m11 - m00 - m22);
                    x = (m01 + m10) * s;
                    y = T(0.25) / s;
                    z = (m12 + m21) * s;
                    w = (m02 - m20) * s;
                } 
                else {
                    T s = T(0.5) / std::sqrt(1 + m22 - m00 - m11);
                    x = (m02 + m20) * s;
                    y = (m12 + m21) * s;
                    z = T(0.25) / s;
                    w = (m10 - m01) * s;
                }
            }
            m_Vector = Vector<T, 4>{x, y, z, w};
        }

        Quaternion operator-() const { return Quaternion(Vector<T, 4>{-m_Vector.Get(0),-m_Vector.Get(1),-m_Vector.Get(2),-m_Vector.Get(3)}); }
        Quaternion operator~() const { return Quaternion(Vector<T, 4>{-m_Vector.Get(0),-m_Vector.Get(1),-m_Vector.Get(2),m_Vector.Get(3)}); }
        Quaternion& operator*=(const Quaternion& other) noexcept 
        {
            auto a = m_Vector;
            auto b = other.m_Vector;
            m_Vector.Set(0, a.Get(3)*b.Get(0) + a.Get(0)*b.Get(3) + a.Get(1)*b.Get(2) - a.Get(2)*b.Get(1)); // x
            m_Vector.Set(1, a.Get(3)*b.Get(1) - a.Get(0)*b.Get(2) + a.Get(1)*b.Get(3) + a.Get(2)*b.Get(0)); // y
            m_Vector.Set(2, a.Get(3)*b.Get(2) + a.Get(0)*b.Get(1) - a.Get(1)*b.Get(0) + a.Get(2)*b.Get(3)); // z
            m_Vector.Set(3, a.Get(3)*b.Get(3) - a.Get(0)*b.Get(0) - a.Get(1)*b.Get(1) - a.Get(2)*b.Get(2)); // w
            return *this;
        }

        inline bool operator==(const Quaternion& o) { return m_Vector == o.m_Vector; } 

        inline Scalar<T> Get(size_t index) const noexcept { return m_Vector.Get(index); }
        inline void Set(size_t index, T val) noexcept { m_Vector.Set(index, val); }
        // 四元数转欧拉角（Pitch, Yaw, Roll），返回Vector<T, 3>，单位为弧度
        Vector<T, 3> ToEulerAngles() const
        {
            float x = m_Vector.Get(0);
            float y = m_Vector.Get(1);
            float z = m_Vector.Get(2);
            float w = m_Vector.Get(3);

            // pitch (X轴)
            float sinX = 2.0f * (w * x - y * z);
            sinX = sinX > 1.0f ? 1.0f : sinX;
            sinX = sinX < -1.0f ? -1.0f : sinX;
            float pitch = std::asin(sinX);

            // roll (Y轴)
            float sinY_cosX = 2.0f * (w * y + x * z);
            float cosY_cosX = 1.0f - 2.0f * (x * x + y * y);
            float yaw = std::atan2(sinY_cosX, cosY_cosX);

            // yaw (Z轴)
            float sinZ_cosX = 2.0f * (w * z + x * y);
            float cosZ_cosX = 1.0f - 2.0f * (x * x + z * z);
            float roll = std::atan2(sinZ_cosX, cosZ_cosX);

            return Vector<T, 3>{pitch, yaw, roll};
        }

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

        Vector<T,4> m_Vector; // x,y,z,w
    };

    template<typename T> requires std::is_arithmetic_v<T>
    inline Quaternion<T> operator*(Quaternion<T> q0, Quaternion<T> q1)  { return q0 *= q1; }
    template<typename T> requires std::is_arithmetic_v<T>
    inline Vector<T, 3> operator*(Quaternion<T> q, const Vector<T, 3>& vec) 
    {
        Quaternion<T>::Normalize(q);
        Quaternion<T> qv{};
        qv.Set(0, vec.Get(0));
        qv.Set(1, vec.Get(1));
        qv.Set(2, vec.Get(2));
        qv.Set(3, 0);
        Quaternion<T> res = q * qv * (~q);
        return Vector<T, 3>{res.Get(0),res.Get(1),res.Get(2)};
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
        return std::format_to(ctx.out(), "{}, {}, {}, {}\n", k.Get(0), k.Get(1), k.Get(2), k.Get(3));
    }
};


#endif
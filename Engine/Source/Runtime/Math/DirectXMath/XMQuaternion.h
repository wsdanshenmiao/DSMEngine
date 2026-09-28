#pragma once
#ifndef __XMQUATERNION_H__
#define __XMQUATERNION_H__

#include "XMVector.h"

namespace DSM {
    class XMMatrix3;
    class XMMatrix4;
    
    class XMQuaternion
    {
    public:
        inline XMQuaternion() noexcept : m_Vector(DirectX::XMQuaternionIdentity()){}
        inline XMQuaternion(const XMVector3& axis, const XMScalar& angle) noexcept
            :m_Vector(DirectX::XMQuaternionRotationAxis(axis, angle)){}
        // 三个角分别为 俯仰角(x)、偏航角(y)、滚动角(z)
        inline XMQuaternion(float pitch, float yaw, float roll) noexcept : XMQuaternion(XMVector3{pitch, yaw, roll}) {}
        XMQuaternion(XMVector3 v) noexcept;
        inline XMQuaternion(float x, float y, float z, float w) noexcept : m_Vector(DirectX::XMVectorSet(x, y, z, w)) {}
        explicit XMQuaternion(const XMMatrix3& matrix);
        explicit XMQuaternion(const XMMatrix4& matrix);

        inline XMQuaternion operator-() const noexcept { return DirectX::XMVectorNegate(m_Vector); }
        // 返回四元数的共轭
        inline XMQuaternion operator~() const noexcept { return DirectX::XMQuaternionConjugate(m_Vector); }
        inline XMQuaternion& operator*=(const XMQuaternion& other) noexcept
        {
            m_Vector = DirectX::XMQuaternionMultiply(m_Vector, other.m_Vector);
            return *this;
        }

        inline bool operator==(const XMQuaternion& o){ return DirectX::XMQuaternionEqual(m_Vector, o); }
    
        XMScalar Get(size_t index) const;
        // XMVectorPermute 会从两个向量中重新布局新的向量，8个标量分别对应0 - 7 
        void Set(size_t index, XMScalar val);

        // 四元数转欧拉角（Pitch, Yaw, Roll），返回XMVector3，单位为弧度
        XMVector3 ToEulerAngles() const;
        
        inline XMQuaternion Normalized() const noexcept { XMQuaternion ret = *this; Normalize(ret); return ret; }

        inline operator DirectX::XMVECTOR() const noexcept { return m_Vector; }


        inline static XMQuaternion Normalize(XMQuaternion q) noexcept { return XMQuaternion{DirectX::XMQuaternionNormalize(q)}; }
        // 使用球面线性插值来对两个四元数进行插值
        inline static XMQuaternion Slerp(XMQuaternion q0, XMQuaternion q1, float t) noexcept
        {
            return Normalize(XMQuaternion{DirectX::XMQuaternionSlerp(q0, q1, t)});
        }
        inline static XMQuaternion Lerp(XMQuaternion q0, XMQuaternion q1, float t) noexcept
        {
            return Normalize(XMQuaternion{DirectX::XMVectorLerp(q0, q1, t)});
        }
        
    private:
        inline XMQuaternion(DirectX::FXMMATRIX matrix) noexcept : m_Vector(XMQuaternionRotationMatrix(matrix)){}
        inline XMQuaternion(DirectX::FXMVECTOR vector) noexcept : m_Vector(vector){}

    private:
        DirectX::XMVECTOR m_Vector{};
    };

    inline XMQuaternion operator*(XMQuaternion q0, XMQuaternion q1) { return q0 *= q1; }
    // 旋转一个向量
    inline XMVector3 operator*(XMQuaternion q, XMVector3 v)
    {
        DirectX::XMFLOAT3 tmp;
        DirectX::XMStoreFloat3(&tmp, DirectX::XMVector3Rotate(v, q));
        return XMVector3{tmp.x, tmp.y, tmp.z};
    }


}


template<>
struct std::formatter<DSM::XMQuaternion>
{
    template<typename Context>
    constexpr auto parse(Context& ctx) { return ctx.begin(); }

    template<typename Context>
    auto format(const DSM::XMQuaternion& k, Context& ctx) const 
    {
        return std::format_to(ctx.out(), "{}, {}, {}, {}/n", k.Get(0), k.Get(1), k.Get(2), k.Get(3));
    }
};


#endif
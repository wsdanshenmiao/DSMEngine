#pragma once
#ifndef __XMQUATERNION_H__
#define __XMQUATERNION_H__

#include "XMVector.h"

namespace DSM {
    
    class XMQuaternion
    {
    public:
        inline XMQuaternion() noexcept : m_Vector(DirectX::XMQuaternionIdentity()){}
        inline XMQuaternion(const XMVector3& axis, const XMScalar& angle) noexcept
            :m_Vector(DirectX::XMQuaternionRotationAxis(axis, angle)){}
        // 三个角分别为 俯仰角(x)、偏航角(y)、滚动角(z)
        inline XMQuaternion(float pitch, float yaw, float roll) noexcept
            :m_Vector((DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll))){}

        inline XMQuaternion operator-(){ return DirectX::XMVectorNegate(m_Vector); }
        // 返回四元数的共轭
        inline XMQuaternion operator~(){ return DirectX::XMQuaternionConjugate(m_Vector); }
        inline XMQuaternion& operator*=(XMQuaternion other) noexcept
        {
            m_Vector = DirectX::XMQuaternionMultiply(m_Vector, other.m_Vector);
            return *this;
        }

        inline bool operator==(const XMQuaternion& o){ return DirectX::XMQuaternionEqual(m_Vector, o); }
    
        inline XMScalar Get(size_t index) const 
        {
            switch (index) {
            case 0: return XMScalar{DirectX::XMVectorSplatX(m_Vector)}; 
            case 1: return XMScalar{DirectX::XMVectorSplatY(m_Vector)}; 
            case 2: return XMScalar{DirectX::XMVectorSplatZ(m_Vector)};
            case 3: return XMScalar{DirectX::XMVectorSplatW(m_Vector)};
            default:
                throw std::out_of_range("Index out of range.");
            }
            return XMScalar{};
        }
        // XMVectorPermute 会从两个向量中重新布局新的向量，8个标量分别对应0 - 7 
        inline void Set(size_t index, XMScalar x) 
        { 
            switch (index) {
            case 0: m_Vector = DirectX::XMVectorPermute<4,1,2,3>(m_Vector, x);
            case 1: m_Vector = DirectX::XMVectorPermute<0,5,2,3>(m_Vector, x);
            case 2: m_Vector = DirectX::XMVectorPermute<0,1,6,3>(m_Vector, x);
            case 3: m_Vector = DirectX::XMVectorPermute<0,1,2,7>(m_Vector, x);
            default:
                throw std::out_of_range("Index out of range.");
            }
        }
        
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
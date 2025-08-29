#pragma once
#ifndef __MATHCOMMON_H__
#define __MATHCOMMON_H__

#include <concepts>
#include <cassert>
#include "Core/PlatformDetection.h"
#include "Vector.h"

#if defined(DSM_PLATFORM_WINDOWS)
#include "DirectXMath/XMScalar.h"
#include "DirectXMath/XMVector.h"
#include "DirectXMath/XMQuaternion.h"
#include "DirectXMath/XMMatrix.h"
#else
#include "Scalar.h"
#include "Quaternion.h"
#include "Matrix.h"
#endif

namespace DSM::Math {

#if defined(DSM_PLATFORM_WINDOWS)
    using Scalar = XMScalar;
    using Vector3 = XMVector3;
    using Vector4 = XMVector4;
    using Quaternion = XMQuaternion;
    using Matrix3 = XMMatrix3;
    using Matrix4 = XMMatrix4;
#else
    using Scalar = DSM::Scalar<float>;
    using Vector3 = DSM::Vector3f;
    using Vector4 = DSM::Vector4f;
    using Quaternion = DSM::Quaternionf;
    using Matrix3 = Matrix<float, 3, 3>;
    using Matrix4 = Matrix<float, 4, 4>;
#endif
    using Vector2 = DSM::Vector2f;

    inline Matrix4 GetLocalToWorld(const Vector3& pos, const Vector3& scale, const Quaternion& q)
    {
#if defined(DSM_PLATFORM_WINDOWS)
        return Matrix4{DirectX::XMMatrixAffineTransformation(
                scale, DirectX::g_XMZero, q, pos)};
#else
        Matrix4 S = Matrix4::GetScale(scale);
        Matrix4 R = Matrix4::GetRotate(q);
        Matrix4 SRT = S * R;
        SRT.Set(3, 0, pos.Get(0));
        SRT.Set(3, 1, pos.Get(1));
        SRT.Set(3, 2, pos.Get(2));
        return SRT;
#endif
    }
    
    inline Matrix4 GetProjMatrix(float fovAngleY, float aspect, float nearZ, float farZ)
    {
#if defined(DSM_PLATFORM_WINDOWS)
        return Math::Matrix4{DirectX::XMMatrixPerspectiveFovLH(fovAngleY, aspect, nearZ, farZ)};
#else
        float height = 1.f / std::tan(fovAngleY * 0.5f);
        float width = height / aspect;
        float fRange = farZ / (farZ - nearZ);

        Matrix4 m;
        m.Set(0, 0, width);
        m.Set(0, 1, 0);
        m.Set(0, 2, 0);
        m.Set(0, 3, 0);

        m.Set(1, 0, 0);
        m.Set(1, 1, height);
        m.Set(1, 2, 0);
        m.Set(1, 3, 0);

        m.Set(2, 0, 0);
        m.Set(2, 1, 0);
        m.Set(2, 2, fRange);
        m.Set(2, 3, 1);

        m.Set(3, 0, 0);
        m.Set(3, 1, 0);
        m.Set(3, 2, -nearZ * fRange);
        m.Set(3, 3, 0);
        return m;
#endif
    }

    inline Quaternion LookTo(const Vector3& pos, const Vector3& dir, const Vector3& up)
    {
#if defined(DSM_PLATFORM_WINDOWS)
        Matrix4 view{DirectX::XMMatrixLookToLH(pos, dir, up)};
        return Quaternion{Matrix4::Inverse(view)};
#else
        assert(!dir.NearZero());
        assert(!up.NearZero());
            
        Vector3 R2 = dir.Normalized();

        Vector3 R0 = Vector3::Cross(up, R2);
        Vector3::Normalize(R0);

        Vector3 R1 = Vector3::Cross(R2, R0);

        Vector3 NegEyePosition = -pos;

        Vector3 D0 = Vector3::Dot(R0, NegEyePosition);
        Vector3 D1 = Vector3::Dot(R1, NegEyePosition);
        Vector3 D2 = Vector3::Dot(R2, NegEyePosition);

        Matrix4 M;    
        M.Set(0, Vector4{R0.Get(0), R0.Get(1), R0.Get(2), D0.Get(3)});
        M.Set(1, Vector4{R1.Get(0), R1.Get(1), R1.Get(2), D1.Get(3)});
        M.Set(2, Vector4{R2.Get(0), R2.Get(1), R2.Get(2), D2.Get(3)});
        M.Set(3, Vector4{0,0,0,1});

        M = Matrix4::Transpose(M);

        return Quaternion{Matrix4::Inverse(M)};
#endif
    }

    inline Quaternion LookAt(const Vector3& pos, const Vector3& target, const Vector3& up)
    {
#if defined(DSM_PLATFORM_WINDOWS)
        Matrix4 view{DirectX::XMMatrixLookAtLH(pos, target, up)};
        return Quaternion{Matrix4::Inverse(view)};
#else
        Vector3 negEyeDirection = pos - target;
        return LookTo(pos, negEyeDirection, up);
#endif
    }



    template <std::unsigned_integral T>
    inline T NextPowerOf2(T val)
    {
        val--;
        val |= val >> 1;
        val |= val >> 2;
        val |= val >> 4;
        val |= val >> 8;
        val |= val >> 16;
        val++;

        return val;
    }

    template<typename T> requires std::is_unsigned_v<T>
    inline T Align(T size, T alignment)
    {
        if(alignment <= 1) return size;
        else return (size + alignment - 1) & ~(alignment - 1);
    }

    template <typename T> requires std::is_arithmetic_v<T>
    inline constexpr T DivideByMultiple(T value, std::uint64_t alignment) noexcept
    {
        return (T)((value + alignment - 1) / alignment);
    }

} // namespace DSM 

#endif
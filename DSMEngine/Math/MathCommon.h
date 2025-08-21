#pragma once
#ifndef __MATHCOMMON_H__
#define __MATHCOMMON_H__

#include <concepts>
#include "Vector.h"

#if defined(DSM_PLATFORM_WINDOWS)
#include "XMScalar.h"
#include "XMVector.h"
#include "XMQuaternion.h"
#else
#include "Scalar.h"
#include "Quaternion.h"
#endif

namespace DSM::Math {
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


#if defined(DSM_PLATFORM_WINDOWS)
    using Scalar = XMScalar;
    using Vector3 = XMVector3;
    using Vector4 = XMVector4;
    using Quaternion = XMQuaternion;
#else
    using Scalar = DSM::Scalar<float>;
    using Vector3 = DSM::Vector3f;
    using Vector4 = DSM::Vector4f;
    using Quaternion = DSM::Quaternion;
#endif
    using Vector2 = DSM::Vector2f;

} // namespace DSM 

#endif
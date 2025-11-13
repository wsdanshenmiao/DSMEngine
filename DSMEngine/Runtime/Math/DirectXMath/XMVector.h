#pragma once
#ifndef __XMVECTOR_H__
#define __XMVECTOR_H__

#include <span>
#include <stdexcept>
#include <utility>
#include "XMScalar.h"

namespace DSM {
    class XMVector4;
    
    class XMVector3
    {
        friend class XMMatrix3;
        friend class XMQuaternion;
    public:
        inline XMVector3() noexcept : m_Vector(DirectX::XMVectorReplicate(0)) {}
        inline XMVector3(float s) noexcept : m_Vector(XMScalar{s}) {}
        inline XMVector3(XMScalar s) noexcept : m_Vector(s) {}
        inline XMVector3(std::span<const float, 3> data) :XMVector3(DirectX::XMFLOAT3{data[0], data[1], data[2]}) {}
        inline XMVector3(std::initializer_list<float> initList)
        {
            DirectX::XMFLOAT3 tmp{};
            memcpy(&tmp, initList.begin(), sizeof(float) * (std::min)(initList.size(), 3llu));
            m_Vector = DirectX::XMLoadFloat3(&tmp);
        }
        inline explicit XMVector3(const XMVector4& v);

        inline XMVector3 operator-() const noexcept { return DirectX::XMVectorNegate(m_Vector); }

        inline XMVector3& operator+=(XMVector3 other) noexcept
        {
            m_Vector = DirectX::XMVectorAdd(m_Vector, other);
            return *this;
        }
        inline XMVector3& operator-=(XMVector3 other) noexcept
        {
            m_Vector = DirectX::XMVectorSubtract(m_Vector, other);
            return *this;
        }
        inline XMVector3& operator*=(XMVector3 scalar) noexcept
        {
            m_Vector = DirectX::XMVectorMultiply(m_Vector, scalar);
            return *this;
        }
        inline XMVector3& operator*=(XMScalar scalar) noexcept { return operator*=(XMVector3(scalar)); }
        inline XMVector3& operator*=(float v) noexcept { return operator*=(XMScalar{v}); }
        inline XMVector3& operator/=(XMVector3 scalar) noexcept
        {
            m_Vector = DirectX::XMVectorDivide(m_Vector, scalar);
            return *this;
        }
        inline XMVector3& operator/=(XMScalar scalar) noexcept { return operator/=(XMVector3(scalar)); }
        inline XMVector3& operator/=(float v) noexcept { return operator/=(XMScalar{v}); }
        
        inline bool operator==(const XMVector3& other) const noexcept { return DirectX::XMVector3Equal(m_Vector, other); }

        friend XMVector3 operator*(const XMVector3& v, const XMMatrix3& m) noexcept;

        inline XMScalar Get(size_t index) const 
        {
            switch (index) {
            case 0: return XMScalar{DirectX::XMVectorSplatX(m_Vector)}; 
            case 1: return XMScalar{DirectX::XMVectorSplatY(m_Vector)}; 
            case 2: return XMScalar{DirectX::XMVectorSplatZ(m_Vector)};
            default:
                throw std::out_of_range("Index out of range.");
            }
            return XMScalar{};
        }
        // XMVectorPermute 会从两个向量中重新布局新的向量，8个标量分别对应0 - 7 
        inline void Set(size_t index, XMScalar x) 
        { 
            switch (index) {
            case 0: m_Vector = DirectX::XMVectorPermute<4,1,2,3>(m_Vector, x); break;
            case 1: m_Vector = DirectX::XMVectorPermute<0,5,2,3>(m_Vector, x); break;
            case 2: m_Vector = DirectX::XMVectorPermute<0,1,6,3>(m_Vector, x); break;
            default:
                throw std::out_of_range("Index out of range.");
            }
        }
        inline void Set(size_t index, float val) { Set(index, XMScalar{val}); }
        
        std::size_t Size() const noexcept { return 3; }
        void Fill(float v) noexcept { m_Vector = DirectX::XMVectorReplicate(v); }
        XMScalar SqrMagnitude() const noexcept { return XMScalar{ DirectX::XMVector3LengthSq(m_Vector) }; }
        XMScalar Magnitude() const noexcept { return XMScalar{ DirectX::XMVector3Length(m_Vector) }; }
        XMVector3 Normalized() const noexcept { return XMVector3{ DirectX::XMVector3Normalize(m_Vector) }; }
        // 检测该向量是否接近零向量，避免边缘情况
        bool NearZero() const { return std::abs(Get(0)) < 1e-6f && std::abs(Get(1)) < 1e-6f && std::abs(Get(2)) < 1e-6f; }

        inline operator DirectX::XMVECTOR() const noexcept { return m_Vector; }

        static inline XMVector3 Abs(XMVector3 v) noexcept { return DirectX::XMVectorAbs(v); }
        static inline void Normalize(XMVector3& v) noexcept { DirectX::XMVector3Normalize(v.m_Vector); }
        static inline XMScalar Distance(const XMVector3& v1, const XMVector3& v2) noexcept;
        static inline XMVector3 Zero() noexcept { return XMVector3{}; }
        static inline XMVector3 One() noexcept { return XMVector3{1}; }
        static inline XMVector3 NegativeInfinity() noexcept { return XMVector3{std::numeric_limits<float>::lowest()}; }
        static inline XMVector3 PositiveInfinity() noexcept { return XMVector3{(std::numeric_limits<float>::max)()}; }
        // 限制向量在某个长度
        static inline XMVector3 ClampMagnitude(XMVector3 v, XMScalar maxLen) noexcept { DirectX::XMVector3ClampLengthV(v, XMScalar{0}, maxLen); }
        static inline XMVector3 Lerp(XMVector3 v1, XMVector3 v2, XMScalar t) noexcept { DirectX::XMVectorLerp(v1, v2, t); }
        // 所有位取两个向量的最大值
        static inline XMVector3 Max(XMVector3 v1, XMVector3 v2) noexcept { return DirectX::XMVectorMax(v1, v2); }
        // 所有位取两个向量的最小值
        static inline XMVector3 Min(XMVector3 v1, XMVector3 v2) noexcept { return DirectX::XMVectorMin(v1, v2); }
        // 将向量v1投影到v2
        static inline XMVector3 Project(const XMVector3& v1, const XMVector3& v2) noexcept;
        static inline XMVector3 Reflect(XMVector3 v, XMVector3 n) noexcept { return DirectX::XMVector3Reflect(v, n); }
        // 根据法线和折射率计算折射光线
        static inline XMVector3 Refract(XMVector3 v, XMVector3 n, float refractiveIndex) noexcept { return DirectX::XMVector3Refract(v, n, refractiveIndex); }
        static inline XMVector3 Cross(XMVector3 v1, XMVector3 v2) noexcept { return DirectX::XMVector3Cross(v1, v2); }
        static inline XMScalar Dot(XMVector3 v1, XMVector3 v2) noexcept { return DirectX::XMVector3Dot(v1, v2); }
        static inline XMVector3 Floor(XMVector3 v) noexcept { return DirectX::XMVectorFloor(v); }

    private:
        inline XMVector3(const DirectX::XMFLOAT3& v) noexcept : m_Vector(DirectX::XMLoadFloat3(&v)) {}
        inline XMVector3(DirectX::FXMVECTOR v) noexcept : m_Vector(v) {}

    private:
        DirectX::XMVECTOR m_Vector{};
    };

    inline XMVector3 operator+(XMVector3 v0, const XMVector3& v1) noexcept { return v0 += v1; };
    inline XMVector3 operator-(XMVector3 v0, const XMVector3& v1) noexcept { return v0 -= v1; };
    inline XMVector3 operator*(XMVector3 v0, const XMVector3& v1) noexcept { return v0 *= v1; };
    inline XMVector3 operator*(XMVector3 v0, XMScalar scalar) noexcept { return v0 *= scalar; };
    inline XMVector3 operator*(XMScalar scalar, XMVector3 v) noexcept { return v *= scalar; };
    inline XMVector3 operator*(XMVector3 v0, float scalar) noexcept { return v0 *= scalar; };
    inline XMVector3 operator*(float scalar, XMVector3 v) noexcept { return v *= scalar; };
    inline XMVector3 operator/(XMVector3 v0, const XMVector3& v1) noexcept { return v0 /= v1; };
    inline XMVector3 operator/(XMVector3 v0, XMScalar scalar) noexcept { return v0 /= scalar; };
    inline XMVector3 operator/(XMVector3 v, float scalar) noexcept { return v /= scalar; };
    
    XMScalar XMVector3::Distance(const XMVector3& v1, const XMVector3& v2) noexcept { return (v2 - v1).Magnitude(); }
    XMVector3 XMVector3::Project(const XMVector3& v1, const XMVector3& v2) noexcept { return (XMVector3::Dot(v1, v2) / v2.SqrMagnitude()) * v2; }



    class XMVector4
    {
        friend class XMMatrix4;
        friend class XMQuaternion;
    public:
        inline XMVector4() noexcept : m_Vector(DirectX::XMVectorReplicate(0)) {}
        inline XMVector4(float s) noexcept : m_Vector(XMScalar{s}) {}
        inline XMVector4(XMScalar s) noexcept : m_Vector(s) {}
        inline XMVector4(std::span<const float, 4> data) :XMVector4(DirectX::XMFLOAT4{data[0], data[1], data[2], data[3]}) {}
        inline XMVector4(std::initializer_list<float> initList)
        {
            DirectX::XMFLOAT4 tmp{};
            memcpy(&tmp, initList.begin(), sizeof(float) * (std::min)(initList.size(), 4llu));
            m_Vector = DirectX::XMLoadFloat4(&tmp);
        }
        inline explicit XMVector4(const XMVector3& v) :m_Vector(DirectX::XMVectorSetW(v, 0)) {}
        inline explicit XMVector4(const XMVector3& v, float val) :m_Vector(DirectX::XMVectorSetW(v, val)) {}

        inline XMVector4 operator-() const noexcept { return DirectX::XMVectorNegate(m_Vector); }

        inline XMVector4& operator+=(XMVector4 other) noexcept
        {
            m_Vector = DirectX::XMVectorAdd(m_Vector, other);
            return *this;
        }
        inline XMVector4& operator-=(XMVector4 other) noexcept
        {
            m_Vector = DirectX::XMVectorSubtract(m_Vector, other);
            return *this;
        }
        inline XMVector4& operator*=(XMVector4 scalar) noexcept
        {
            m_Vector = DirectX::XMVectorMultiply(m_Vector, scalar);
            return *this;
        }
        inline XMVector4& operator*=(XMScalar scalar) noexcept { return operator*=(XMVector4(scalar)); }
        inline XMVector4& operator*=(float v) noexcept { return operator*=(XMScalar{v}); }
        inline XMVector4& operator/=(XMVector4 scalar) noexcept
        {
            m_Vector = DirectX::XMVectorDivide(m_Vector, scalar);
            return *this;
        }
        inline XMVector4& operator/=(XMScalar scalar) noexcept { return operator/=(XMVector4(scalar)); }
        inline XMVector4& operator/=(float v) noexcept { return operator/=(XMScalar{v}); }
        
        inline bool operator==(const XMVector4& other) noexcept { return DirectX::XMVector4Equal(m_Vector, other); }

        friend XMVector4 operator*(XMVector4 v, const XMMatrix4& m) noexcept;

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
            case 0: m_Vector = DirectX::XMVectorPermute<4, 1, 2, 3>(m_Vector, x); break;
            case 1: m_Vector = DirectX::XMVectorPermute<0,5,2,3>(m_Vector, x); break;
            case 2: m_Vector = DirectX::XMVectorPermute<0,1,6,3>(m_Vector, x); break;
            case 3: m_Vector = DirectX::XMVectorPermute<0,1,2,7>(m_Vector, x); break;
            default:
                throw std::out_of_range("Index out of range.");
            }
        }
        
        std::size_t Size() const noexcept { return 4; }
        void Fill(float v) noexcept { m_Vector = DirectX::XMVectorReplicate(v); }
        XMScalar SqrMagnitude() const noexcept { return XMScalar{ DirectX::XMVector4LengthSq(m_Vector) }; }
        XMScalar Magnitude() const noexcept { return XMScalar{ DirectX::XMVector4Length(m_Vector) }; }
        XMVector4 Normalized() const noexcept { return XMVector4{ DirectX::XMVector4Normalize(m_Vector) }; }
        // 检测该向量是否接近零向量，避免边缘情况
        bool NearZero() const { return std::abs(Get(0)) < 1e-6f && std::abs(Get(1)) < 1e-6f && std::abs(Get(2)) < 1e-6f && std::abs(Get(3)) < 1e-6f; }

        inline operator DirectX::XMVECTOR() const noexcept { return m_Vector; }

        static inline XMVector4 Abs(XMVector4 v) noexcept { return DirectX::XMVectorAbs(v); }
        static inline void Normalize(XMVector4& v) noexcept { DirectX::XMVector4Normalize(v.m_Vector); }
        static inline XMScalar Distance(XMVector4 v1, XMVector4 v2) noexcept;
        static inline XMVector4 Zero() noexcept { return XMVector4{}; }
        static inline XMVector4 One() noexcept { return XMVector4{1}; }
        static inline XMVector4 NegativeInfinity() noexcept { return XMVector4{std::numeric_limits<float>::lowest()}; }
        static inline XMVector4 PositiveInfinity() noexcept { return XMVector4{(std::numeric_limits<float>::max)()}; }
        // 限制向量在某个长度
        static inline XMVector4 ClampMagnitude(XMVector4 v, XMScalar maxLen) noexcept { DirectX::XMVector4ClampLength(v, 0, maxLen); }
        static inline XMVector4 Lerp(XMVector4 v1, XMVector4 v2, XMScalar t) noexcept { DirectX::XMVectorLerp(v1, v2, t); }
        // 所有位取两个向量的最大值
        static inline XMVector4 Max(XMVector4 v1, XMVector4 v2) noexcept { return DirectX::XMVectorMax(v1, v2); }
        // 所有位取两个向量的最小值
        static inline XMVector4 Min(XMVector4 v1, XMVector4 v2) noexcept { return DirectX::XMVectorMin(v1, v2); }
        // 将向量v1投影到v2
        static inline XMVector4 Project(XMVector4 v1, XMVector4 v2) noexcept;
        static inline XMVector4 Reflect(XMVector4 v, XMVector4 n) noexcept { return DirectX::XMVector4Reflect(v, n); }
        // 根据法线和折射率计算折射光线
        static inline XMVector4 Refract(XMVector4 v, XMVector4 n, float refractiveIndex) noexcept { return DirectX::XMVector4Refract(v, n, refractiveIndex); }
        //static inline XMVector4 Cross(XMVector4 v1, XMVector4 v2) noexcept { return DirectX::XMVector4Cross(v1, v2); }
        static inline XMScalar Dot(XMVector4 v1, XMVector4 v2) noexcept { return DirectX::XMVector4Dot(v1, v2); }


    private:
        inline XMVector4(const DirectX::XMFLOAT4& v) noexcept : m_Vector(DirectX::XMLoadFloat4(&v)) {}
        inline XMVector4(DirectX::FXMVECTOR v) noexcept : m_Vector(v) {}

    private:
        DirectX::XMVECTOR m_Vector{};
    };

    inline XMVector4 operator+(XMVector4 v0, XMVector4 v1) noexcept { return v0 += v1; };
    inline XMVector4 operator-(XMVector4 v0, XMVector4 v1) noexcept { return v0 -= v1; };
    inline XMVector4 operator*(XMVector4 v0, XMVector4 v1) noexcept { return v0 *= v1; };
    inline XMVector4 operator*(XMVector4 v0, XMScalar scalar) noexcept { return v0 *= scalar; };
    inline XMVector4 operator*(XMScalar scalar, XMVector4 v) noexcept { return v *= scalar; };
    inline XMVector4 operator*(XMVector4 v0, float scalar) noexcept { return v0 *= scalar; };
    inline XMVector4 operator*(float scalar, XMVector4 v) noexcept { return v *= scalar; };
    inline XMVector4 operator/(XMVector4 v0, XMVector4 v1) noexcept { return v0 /= v1; };
    inline XMVector4 operator/(XMVector4 v0, XMScalar scalar) noexcept { return v0 /= scalar; };
    inline XMVector4 operator/(XMVector4 v, float scalar) noexcept { return v /= scalar; };

    XMScalar XMVector4::Distance(XMVector4 v1, XMVector4 v2) noexcept { return (v2 - v1).Magnitude(); }
    XMVector4 XMVector4::Project(XMVector4 v1, XMVector4 v2) noexcept { return (XMVector4::Dot(v1, v2) / v2.SqrMagnitude()) * v2; }


    inline XMVector3::XMVector3(const XMVector4& v) : m_Vector(DirectX::XMVECTOR(v)) {}
}

template<>
struct std::formatter<DSM::XMVector3>
{
    template<typename Context>
    constexpr auto parse(Context& ctx) { return ctx.begin(); }

    template<typename Context>
    auto format(const DSM::XMVector3& k, Context& ctx) const 
    {
        return std::format_to(ctx.out(), "{}, {}, {}/n", k.Get(0), k.Get(1), k.Get(2));
    }
};

template<>
struct std::formatter<DSM::XMVector4>
{
    template<typename Context>
    constexpr auto parse(Context& ctx) { return ctx.begin(); }

    template<typename Context>
    auto format(const DSM::XMVector4& k, Context& ctx) const 
    {
        return std::format_to(ctx.out(), "{}, {}, {}, {}/n", k.Get(0), k.Get(1), k.Get(2), k.Get(3));
    }
};


#endif
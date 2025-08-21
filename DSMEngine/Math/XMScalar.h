#pragma once
#ifndef __XMScalar_H__
#define __XMScalar_H__

#include <DirectXMath.h>
#include <compare>
#include <format>

namespace DSM {
    
    class XMScalar
    {
        friend class XMVector3;
        friend class XMVector4;
        friend class XMQuaternion;
    public:
        inline XMScalar() noexcept { m_Vector = DirectX::XMVectorReplicate(0);}
        inline XMScalar(float v) noexcept { m_Vector = DirectX::XMVectorReplicate(v);}

        inline XMScalar& operator-() noexcept{ m_Vector = DirectX::XMVectorNegate(m_Vector); return *this; }
        inline XMScalar& operator+=(XMScalar other) noexcept
        {
            m_Vector = DirectX::XMVectorAdd(m_Vector, other);
            return *this;
        }
        inline XMScalar& operator+=(float v) noexcept{return operator+=(XMScalar{v});}
        
        inline XMScalar& operator-=(XMScalar other) noexcept
        {
            m_Vector = DirectX::XMVectorSubtract(m_Vector, other);
            return *this;
        }
        inline XMScalar& operator-=(float v) noexcept { return operator-=(XMScalar{v}); }
        
        inline XMScalar& operator*=(XMScalar other) noexcept
        {
            DirectX::XMVectorMultiply(m_Vector, other);
            return *this;
        }
        inline XMScalar& operator*=(float v) noexcept { return operator*=(XMScalar{v}); }

        inline XMScalar& operator/=(XMScalar other) noexcept
        {
            DirectX::XMVectorDivide(m_Vector, other);
            return *this;
        }
        inline XMScalar& operator/=(float v) noexcept { return operator/=(XMScalar{v}); }

        inline operator DirectX::XMVECTOR() const noexcept { return m_Vector; }
        inline operator float() const noexcept { return DirectX::XMVectorGetX(m_Vector); }

    private:
        inline XMScalar(DirectX::FXMVECTOR v) noexcept : m_Vector(v) {}

    private:
        DirectX::XMVECTOR m_Vector{};
    };

    inline XMScalar operator+(XMScalar s0, XMScalar s1) noexcept{ return s0 += s1; }
    inline XMScalar operator+(XMScalar s0, float s1) noexcept{ return s0 += s1; }
    inline XMScalar operator+(float s0, XMScalar s1) noexcept{ return XMScalar(s0) += s1; }
    inline XMScalar operator-(XMScalar s0, XMScalar s1) noexcept{ return s0 -= s1; }
    inline XMScalar operator-(XMScalar s0, float s1) noexcept{ return s0 -= s1; }
    inline XMScalar operator-(float s0, XMScalar s1) noexcept{ return XMScalar(s0) -= s1; }
    inline XMScalar operator*(XMScalar s0, XMScalar s1) noexcept{ return s0 *= s1; }
    inline XMScalar operator*(XMScalar s0, float s1) noexcept{ return s0 *= s1; }
    inline XMScalar operator*(float s0, XMScalar s1) noexcept{ return XMScalar(s0 *= s1); }
    inline XMScalar operator/(XMScalar s0, XMScalar s1) noexcept{ return s0 /= s1; }
    inline XMScalar operator/(XMScalar s0, float s1) noexcept{ return s0 /= s1; }
    inline XMScalar operator/(float s0, XMScalar s1) noexcept{ return XMScalar(s0) /= s1; }

    
    inline std::partial_ordering operator<=>(XMScalar s0, XMScalar s1) noexcept { return float(s0) <=> float(s1); }
    inline std::partial_ordering operator<=>(XMScalar s0, float s1) noexcept { return float(s0) <=> s1; }
    inline std::partial_ordering operator<=>(float s0, XMScalar s1) noexcept { return s0 <=> float(s1); }
    inline bool operator==(XMScalar s0, XMScalar s1) noexcept { return float(s0) == float(s1); }
    inline bool operator==(XMScalar s0, float s1) noexcept { return float(s0) == s1; }
    inline bool operator==(float s0, XMScalar s1) noexcept { return s0 == float(s1); }
}

template<>
struct std::formatter<DSM::XMScalar>
{
    template<typename Context>
    constexpr auto parse(Context& ctx) { return ctx.begin(); }

    template<typename Context>
    auto format(const DSM::XMScalar& k, Context& ctx) const 
    {
        return std::format_to(ctx.out(), "{}", float(k));
    }
};

#endif
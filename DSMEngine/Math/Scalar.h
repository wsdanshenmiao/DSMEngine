#pragma once
#ifndef __Scalar_H__
#define __Scalar_H__

#include <format>
#include <concepts>
#include <compare>

namespace DSM {
    template<typename T> requires std::is_arithmetic_v<T>
    class Scalar
    {
    public:
        Scalar() = default;
        Scalar(T val) :m_Data(val) {}
        
        Scalar operator-() const noexcept{ return -m_Data; }

        Scalar& operator+=(Scalar other) noexcept
        {
            m_Data += other.m_Data;
            return *this;
        }
        Scalar& operator+=(float v) noexcept{return operator+=(Scalar{v});}
        
        Scalar& operator-=(Scalar other) noexcept
        {
            m_Data -= other.m_Data;
            return *this;
        }
        Scalar& operator-=(float v) noexcept { return operator-=(Scalar{v}); }
        
        Scalar& operator*=(Scalar other) noexcept
        {
            m_Data *= other.m_Data;
            return *this;
        }
        Scalar& operator*=(float v) noexcept { return operator*=(Scalar{v}); }

        Scalar& operator/=(Scalar other) noexcept
        {
            m_Data /= other.m_Data;
            return *this;
        }
        Scalar& operator/=(float v) noexcept { return operator/=(Scalar{v}); }
        
        inline operator T() const noexcept { return m_Data; }

    private:
        T m_Data{};
    };


    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator+(Scalar<T> s0, Scalar<T> s1) noexcept{ return s0 += s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator+(Scalar<T> s0, float s1) noexcept{ return s0 += s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator+(float s0, Scalar<T> s1) noexcept{ return Scalar<T>(s0) += s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator-(Scalar<T> s0, Scalar<T> s1) noexcept{ return s0 -= s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator-(Scalar<T> s0, float s1) noexcept{ return s0 -= s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator-(float s0, Scalar<T> s1) noexcept{ return Scalar<T>(s0) -= s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator*(Scalar<T> s0, Scalar<T> s1) noexcept{ return s0 *= s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator*(Scalar<T> s0, float s1) noexcept{ return s0 *= s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator*(float s0, Scalar<T> s1) noexcept{ return Scalar<T>(s0) *= s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator/(Scalar<T> s0, Scalar<T> s1) noexcept{ return s0 /= s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator/(Scalar<T> s0, float s1) noexcept{ return s0 /= s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    Scalar<T> operator/(float s0, Scalar<T> s1) noexcept{ return Scalar<T>(s0) /= s1; }

    
    template<typename T> requires std::is_arithmetic_v<T>
    std::partial_ordering operator<=>(Scalar<T> s0, Scalar<T> s1) noexcept { return float(s0) <=> float(s1); }
    template<typename T> requires std::is_arithmetic_v<T>
    std::partial_ordering operator<=>(Scalar<T> s0, float s1) noexcept { return float(s0) <=> s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    std::partial_ordering operator<=>(float s0, Scalar<T> s1) noexcept { return s0 <=> float(s1); }
    template<typename T> requires std::is_arithmetic_v<T>
    bool operator==(Scalar<T> s0, Scalar<T> s1) noexcept { return float(s0) == float(s1); }
    template<typename T> requires std::is_arithmetic_v<T>
    bool operator==(Scalar<T> s0, float s1) noexcept { return float(s0) == s1; }
    template<typename T> requires std::is_arithmetic_v<T>
    bool operator==(float s0, Scalar<T> s1) noexcept { return s0 == float(s1); }
    
    using Scalarf = Scalar<float>;
    using Scalard = Scalar<double>;
    using Scalari = Scalar<int>;

} // namespace DSM 

template<typename T>
struct std::formatter<DSM::Scalar<T>>
{
    template<typename Context>
    constexpr auto parse(Context& ctx) { return ctx.begin(); }

    template<typename Context>
    auto format(const DSM::Scalar<T>& k, Context& ctx) const 
    {
        return std::format_to(ctx.out(), "{}", T(k));
    }
};

#endif
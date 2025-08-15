#ifndef __VECTOR__H__
#define __VECTOR__H__

#include <iostream>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>

namespace DSM{
    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    class Vector
    {
    public:
        using UnderlyingType = std::array<T, N>;
        
        constexpr Vector() noexcept;
        constexpr Vector(const T& value) noexcept;
        constexpr Vector(std::initializer_list<T> initList) noexcept;
        constexpr Vector(const std::array<T, N>& data) noexcept;

        auto& operator+=(const Vector& other) noexcept;
        auto& operator-=(const Vector& other) noexcept;
        auto& operator*=(const T& v) noexcept;
        auto& operator/=(const T& v) noexcept;
        T& operator[](const std::size_t& i);
        const T& operator[](const std::size_t& i) const;

        constexpr bool operator==(const Vector& other);

        constexpr std::size_t size() const noexcept;
        constexpr void Fill(const T& v) noexcept;
        constexpr T SqrMagnitude() const noexcept;
        constexpr T Magnitude() const noexcept;
        constexpr Vector Normalized() const noexcept;
        // 检测该向量是否接近零向量，避免边缘情况
        constexpr bool NearZero() const;

        static constexpr void Normalize(Vector& v) noexcept;
        static constexpr T Distance(const Vector& v1, const Vector& v2) noexcept;
        static constexpr Vector Zero() noexcept;
        static constexpr Vector One() noexcept;
        static constexpr Vector NegativeInfinity() noexcept;
        static constexpr Vector PositiveInfinity() noexcept;
        // 限制向量在某个长度
        static constexpr Vector ClampMagnitude(const Vector& v, const T& maxLen) noexcept;
        static constexpr Vector Lerp(const Vector& v1, const Vector& v2, const T& t) noexcept;
        // 所有位取两个向量的最大值
        static constexpr Vector Max(const Vector& v1, const Vector& v2) noexcept;
        // 所有位取两个向量的最小值
        static constexpr Vector Min(const Vector& v1, const Vector& v2) noexcept;
        // 将向量v1投影到v2
        static constexpr Vector Project(const Vector& v1, const Vector& v2);
        static constexpr Vector Scale(const Vector& v1, const Vector& v2) noexcept;
        static constexpr Vector Scale(const Vector& v, const T& s) noexcept;
        static constexpr Vector Reflect(const Vector& v, const Vector& n) noexcept;
        // 根据法线和折射率计算折射光线
        static constexpr Vector Refract(const Vector& v, const Vector& n, float refractiveIndex) noexcept;
        static constexpr Vector Cross(const Vector& v1, const Vector& v2) noexcept;
        static constexpr T Dot(const Vector& v1, const Vector& v2) noexcept;
        
    private:
        std::array<T, N> m_Data;
    };
    
    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N>::Vector() noexcept
    {
        m_Data.fill(0);
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N>::Vector(const T& value) noexcept
    {
        m_Data.fill(value);
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N>::Vector(std::initializer_list<T> initList) noexcept
    {
        auto minSize = std::min(initList.size(), N);
        memcpy(m_Data.data(), initList.begin(), minSize * sizeof(T));
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N>::Vector(const std::array<T, N>& data) noexcept
        :m_Data(data){
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr std::size_t Vector<T, N>::size() const noexcept
    {
        return m_Data.size();
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr void Vector<T, N>::Fill(const T& v) noexcept
    {
        m_Data.fill(v);
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    auto& Vector<T, N>::operator+=(const Vector& other) noexcept
    {
        for (std::size_t i = 0; i < N; ++i) {
            m_Data[i] += other.m_Data[i];
        }
        return *this;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    auto& Vector<T, N>::operator-=(const Vector& other) noexcept
    {
        for (std::size_t i = 0; i < N; ++i) {
            m_Data[i] -= other.m_Data[i];
        }
        return *this;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    auto& Vector<T, N>::operator*=(const T& v) noexcept
    {
        for (auto& elem : m_Data) {
            elem *= v;
        }
        return *this;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    auto& Vector<T, N>::operator/=(const T& v) noexcept
    {
        for (auto& elem : m_Data) {
            elem /= v;
        }
        return *this;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    T& Vector<T, N>::operator[](const std::size_t& i)
    {
        return m_Data[i];
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    const T& Vector<T, N>::operator[](const std::size_t& i) const
    {
        return m_Data[i];
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr T Vector<T, N>::SqrMagnitude() const noexcept
    {
        T ret{};
        for (const auto& elem : m_Data) {
            ret += elem * elem;
        }
        return ret;
    }
    
    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr T Vector<T, N>::Magnitude() const noexcept
    {
        return std::sqrt(SqrMagnitude());
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Normalized() const noexcept
    {
        auto ret = *this;
        Normalize(ret);
        return ret;
    }

    template <typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr bool Vector<T, N>::NearZero() const
    {
        auto s = 1e-6f;
        return std::abs(m_Data[0]) < s && std::abs(m_Data[1]) < s && std::abs(m_Data[2]) < s;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr void Vector<T, N>::Normalize(Vector& v) noexcept
    {
        float magnitude = v.Magnitude();
        if (magnitude <= 1e-6f) return;
        T divisor = 1 / magnitude;
        for (auto& elem : v.m_Data) {
            elem *= divisor;
        }
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr T Vector<T, N>::Distance(const Vector& v1, const Vector& v2) noexcept
    {
        return (v2 - v1).Magnitude();
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Zero() noexcept
    {
        return Vector(0);
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::One() noexcept
    {
        return Vector(1);
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::NegativeInfinity() noexcept
    {
        return Vector(std::numeric_limits<T>::lowest());
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::PositiveInfinity() noexcept
    {
        return Vector(std::numeric_limits<T>::max());
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::ClampMagnitude(const Vector& v, const T& maxLen) noexcept
    {
        return v.SqrMagnitude() > maxLen ? v.Normalized() * maxLen : v;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Lerp(const Vector& v1, const Vector& v2, const T& t) noexcept
    {
        T range = t < 0 ? 0 : (t > 1 ? 1 : t);
        return v1 * (1 - range) + v2 * range;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Max(const Vector& v1, const Vector& v2) noexcept
    {
        Vector ret{};
        for (std::size_t i = 0; i < N; ++i) {
            ret[i] = std::max(v1[i], v2[i]);
        }
        return ret;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Min(const Vector& v1, const Vector& v2) noexcept
    {
        Vector ret{};
        for (std::size_t i = 0; i < N; ++i) {
            ret[i] = std::min(v1[i], v2[i]);
        }
        return ret;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Project(const Vector& v1, const Vector& v2)
    {
        return ((v1 * v2) / v2.SqrMagnitude()) * v2;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Scale(const Vector& v1, const Vector& v2) noexcept
    {
        Vector ret{};
        for (std::size_t i = 0; i < N; ++i) {
            ret[i] = v1[i] * v2[i];
        }
        return ret;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Scale(const Vector& v, const T& s) noexcept
    {
        return v * s;
    }

    template <typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Reflect(const Vector& v, const Vector& n) noexcept
    {
        return v - 2.0f * (v * n) * n;
    }

    template <typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Refract(const Vector& v, const Vector& n, float refractiveIndex) noexcept
    {
        float cosTheta = std::min(1.f, -v * n);
        auto outPerp = refractiveIndex * (v + cosTheta * n);
        auto outParallel = -std::sqrt(std::abs(1.0f - outPerp.SqrMagnitude())) * n;
        return outPerp + outParallel;
    }

    template <typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr Vector<T, N> Vector<T, N>::Cross(const Vector& v1, const Vector& v2) noexcept
    {
        return Vector{v1[1] * v2[2] - v1[2] * v2[1],
                    v1[2] * v2[0] - v1[0] * v2[2],
                    v1[0] * v2[1] - v1[1] * v2[0]};
    }

    template <typename T, std::size_t N> requires std::is_arithmetic_v<T>
    constexpr T Vector<T, N>::Dot(const Vector &v1, const Vector &v2) noexcept
    {
        T ret{0};
        for(std::size_t i = 0; i < N; ++i){
            ret += v1[i] * v2[i];
        }
        return ret;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    Vector<T, N> operator+(const Vector<T, N>& left, const Vector<T, N>& right) noexcept
    {
        Vector<T, N> tmp(left);
        return tmp += right;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    Vector<T, N> operator-(const Vector<T, N>& left, const Vector<T, N>& right) noexcept
    {
        Vector<T, N> tmp(left);
        return tmp -= right;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    Vector<T, N> operator-(const Vector<T, N>& right) noexcept
    {
        Vector<T, N> tmp(0);
        return tmp -= right;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    T operator*(const Vector<T, N>& left, const Vector<T, N>& right)
    {
        T ret{0};
        for (std::size_t i = 0; i < left.size(); ++i) {
            ret += left[i] * right[i];
        }
        return ret;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    Vector<T, N> operator*(const Vector<T, N>& left, const T& right) noexcept
    {
        Vector<T, N> tmp(left);
        return tmp *= right;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    Vector<T, N> operator*(const T& left, const Vector<T, N>& right) noexcept
    {
        Vector<T, N> tmp(right);
        return tmp *= left;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    Vector<T, N> operator/(const Vector<T, N>& left, const T& right) noexcept
    {
        Vector<T, N> tmp(left);
        return tmp /= right;
    }

    template<typename T, std::size_t N> requires std::is_arithmetic_v<T>
    std::ostream& operator<<(std::ostream& out, const Vector<T, N>& v)
    {
        for (int i = 0; i < v.size(); i++) out << v[i] << " ";
        return out;
    }

    
    
    using Vector2f = Vector<float, 2>;
    using Vector3f = Vector<float, 3>;
    using Vector4f = Vector<float, 4>;

    using Vector2d = Vector<double, 2>;
    using Vector3d = Vector<double, 3>;
    using Vector4d = Vector<double, 4>;

    using Vector2i = Vector<int, 2>;
    using Vector3i = Vector<int, 3>;
    using Vector4i = Vector<int, 4>;
    
}

#endif
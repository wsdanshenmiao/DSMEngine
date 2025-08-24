#pragma once
#ifndef __XMMATRIX_H__
#define __XMMATRIX_H__

#include "XMQuaternion.h"
#include <array>

namespace DSM {
    XMVector3 operator*(const XMVector3&, const XMMatrix3&) noexcept;

    // 行主序的矩阵
    __declspec(align(16)) class XMMatrix3
    {
    public:
        inline XMMatrix3() noexcept = default;
        inline XMMatrix3(XMVector3 x, XMVector3 y, XMVector3 z) noexcept : m_Matrix({x,y,z}){}
        inline XMMatrix3(XMQuaternion q) noexcept :XMMatrix3(DirectX::XMMatrixRotationQuaternion(q)) {}

		inline XMMatrix3& operator+=(const XMMatrix3& other) noexcept { for(size_t i = 3; i--; m_Matrix[i] += other.Get(i)); return *this; }
		inline XMMatrix3& operator-=(const XMMatrix3& other) noexcept { for(size_t i = 3; i--; m_Matrix[i] -= other.Get(i)); return *this; }
        inline XMMatrix3& operator*=(const XMMatrix3& m) noexcept { Set(0, Get(0) * m); Set(1, Get(1) * m); Set(2, Get(2) * m); return *this; }
		inline XMMatrix3& operator/=(XMScalar v) noexcept { return operator/=(float(v)); }
		inline XMMatrix3& operator*=(XMScalar v) noexcept { return operator*=(float(v)); }
		inline XMMatrix3& operator/=(float v) noexcept { for(size_t i = 3; i--; m_Matrix[i] /= v); return *this; }
		inline XMMatrix3& operator*=(float v) noexcept { for(size_t i = 3; i--; m_Matrix[i] *= v); return *this; }

        inline bool operator==(const XMMatrix3& m) const noexcept = default;

        inline XMVector3 Get(size_t index) const noexcept{ return m_Matrix[index];}
        inline XMScalar Get(size_t row, size_t col) const noexcept { return Get(row).Get(col); }
        inline void Set(size_t index, XMVector3 x) noexcept { m_Matrix[index] = std::move(x); }
        inline void Set(size_t row, size_t col, XMScalar val) noexcept { Get(row).Set(col, std::move(val)); }
        
        inline operator DirectX::XMMATRIX() const noexcept
        {
            return DirectX::XMMATRIX{m_Matrix[0], m_Matrix[1], m_Matrix[2], DirectX::XMVectorZero()};
        }

        static inline XMMatrix3 GetRotate(XMQuaternion q) noexcept { return XMMatrix3{q}; }
        static inline XMMatrix3 GetRotateX(float angle) noexcept { return XMMatrix3{DirectX::XMMatrixRotationX(angle)}; }
        static inline XMMatrix3 GetRotateY(float angle) noexcept { return XMMatrix3{DirectX::XMMatrixRotationY(angle)}; }
        static inline XMMatrix3 GetRotateZ(float angle) noexcept { return XMMatrix3{DirectX::XMMatrixRotationZ(angle)}; }
        static inline XMMatrix3 GetScale(float s) noexcept { return XMMatrix3{DirectX::XMMatrixScaling(s, s, s)}; }
        static inline XMMatrix3 GetScale(float x, float y, float z) noexcept { return XMMatrix3{DirectX::XMMatrixScaling(x, y, z)}; }
        static inline XMMatrix3 GetScale(XMVector3 s) noexcept { return XMMatrix3{DirectX::XMMatrixScalingFromVector(s)}; }
        static inline XMMatrix3 Inverse(XMMatrix3 m) noexcept
        {
            m.m_Matrix[0] = DirectX::XMVectorSetW(m.Get(0), 0);
            m.m_Matrix[1] = DirectX::XMVectorSetW(m.Get(1), 0);
            m.m_Matrix[2] = DirectX::XMVectorSetW(m.Get(2), 0);
            
            DirectX::XMMATRIX matrix{m.Get(0), m.Get(1), m.Get(2), DirectX::g_XMIdentityR3};
            return DirectX::XMMatrixInverse(nullptr, matrix);
        }
        static inline XMMatrix3 Transpose(XMMatrix3 m) noexcept { return XMMatrix3{DirectX::XMMatrixTranspose(m)}; }
        static inline XMMatrix3 InverseTranspose(XMMatrix3 m) noexcept { return Transpose(Inverse(m)); }

        static const XMMatrix3 Identity;
        
        
    private:
        inline XMMatrix3(DirectX::FXMMATRIX other) noexcept
            :m_Matrix({XMVector3{other.r[0]}, XMVector3{other.r[1]}, XMVector3{other.r[2]}}){}

        std::array<XMVector3, 3> m_Matrix;
    };

	const XMMatrix3 XMMatrix3::Identity = XMMatrix3{ XMVector3{1, 0, 0}, XMVector3{0, 1, 0}, XMVector3{0, 0, 1} };

    inline XMVector3 operator*(const XMVector3& v, const XMMatrix3& m) noexcept  { return DirectX::XMVector3Transform(v, m); }




    __declspec(align(16)) class XMMatrix4
    {
    public:
        inline XMMatrix4() noexcept = default;
        inline XMMatrix4(XMVector3 x, XMVector3 y, XMVector3 z, XMVector3 w) noexcept
        {
            m_Matrix.r[0] = DirectX::XMVectorSetW(x, 0);
            m_Matrix.r[1] = DirectX::XMVectorSetW(y, 0);
            m_Matrix.r[2] = DirectX::XMVectorSetW(z, 0);
            m_Matrix.r[3] = DirectX::XMVectorSetW(w, 1);
        }
        inline XMMatrix4(XMVector4 x, XMVector4 y, XMVector4 z, XMVector4 w) noexcept: m_Matrix({x, y, z, w}) {}
        inline XMMatrix4(const XMMatrix3& m)
        {
            m_Matrix.r[0] = DirectX::XMVectorSetW(m.Get(0), 0);
            m_Matrix.r[1] = DirectX::XMVectorSetW(m.Get(1), 0);
            m_Matrix.r[2] = DirectX::XMVectorSetW(m.Get(2), 0);
            m_Matrix.r[3] = DirectX::g_XMIdentityR3;
        }
        inline XMMatrix4(const XMMatrix3& m, XMVector3 w)
        {
            m_Matrix.r[0] = DirectX::XMVectorSetW(m.Get(0), 0);
            m_Matrix.r[1] = DirectX::XMVectorSetW(m.Get(1), 0);
            m_Matrix.r[2] = DirectX::XMVectorSetW(m.Get(2), 0);
            m_Matrix.r[3] = DirectX::XMVectorSetW(w, 1);
        }
        inline XMMatrix4(XMQuaternion q) noexcept :XMMatrix4(DirectX::XMMatrixRotationQuaternion(q)) {}
        inline XMMatrix4(DirectX::FXMMATRIX matrix) noexcept : m_Matrix(matrix) {}

		inline XMMatrix4& operator+=(const XMMatrix4& other) noexcept { m_Matrix += other; return *this; }
		inline XMMatrix4& operator-=(const XMMatrix4& other) noexcept { m_Matrix -= other; return *this; }
        inline XMMatrix4& operator*=(const XMMatrix4& other) noexcept { m_Matrix *= other; return *this; }
        inline XMMatrix4& operator/=(XMScalar v) noexcept { return operator/=(float(v)); }
		inline XMMatrix4& operator*=(XMScalar v) noexcept { return operator*=(float(v)); }
		inline XMMatrix4& operator/=(float v) noexcept { m_Matrix /= v; return *this; }
		inline XMMatrix4& operator*=(float v) noexcept { m_Matrix *= v; return *this; }

        inline bool operator==(const XMMatrix4& o) const noexcept 
        { 
            for(size_t i = 0; i < 4; ++i){
                if(!DirectX::XMVector4Equal(m_Matrix.r[i], o.Get(i)))
                    return false;
            }
            return true;
        }

        inline XMVector4 Get(size_t index) const noexcept{ return XMVector4{m_Matrix.r[index]};}
        inline XMScalar Get(size_t row, size_t col) const noexcept { return Get(row).Get(col); }
        inline void Set(size_t index, XMVector4 v) noexcept { m_Matrix.r[index] = v; }
        inline void Set(size_t row, size_t col, XMScalar val) noexcept { Get(row).Set(col, std::move(val)); }

        inline operator DirectX::XMMATRIX() const noexcept { return m_Matrix; }

        static inline XMMatrix4 GetRotate(XMQuaternion q) noexcept { return XMMatrix4{q}; }
        static inline XMMatrix4 GetRotateX(float angle) noexcept { return XMMatrix4{DirectX::XMMatrixRotationX(angle)}; }
        static inline XMMatrix4 GetRotateY(float angle) noexcept { return XMMatrix4{DirectX::XMMatrixRotationY(angle)}; }
        static inline XMMatrix4 GetRotateZ(float angle) noexcept { return XMMatrix4{DirectX::XMMatrixRotationZ(angle)}; }
        static inline XMMatrix4 GetScale(float s) noexcept { return XMMatrix4{DirectX::XMMatrixScaling(s, s, s)}; }
        static inline XMMatrix4 GetScale(float x, float y, float z) noexcept { return XMMatrix4{DirectX::XMMatrixScaling(x, y, z)}; }
        static inline XMMatrix4 GetScale(XMVector3 s) noexcept { return XMMatrix4{DirectX::XMMatrixScalingFromVector(s)}; }
        static inline XMMatrix4 Inverse(XMMatrix4 m) noexcept { return XMMatrix4{DirectX::XMMatrixInverse(nullptr, m)}; }
        static inline XMMatrix4 Transpose(XMMatrix4 m) noexcept { return XMMatrix4{DirectX::XMMatrixTranspose(m)}; }
        static inline XMMatrix4 InverseTranspose(XMMatrix4 m) noexcept { return Transpose(Inverse(m)); }
        
		static const XMMatrix4 Identity;

    private:
        inline XMMatrix4(const DirectX::XMFLOAT4X4& f4x4) : m_Matrix(DirectX::XMLoadFloat4x4(&f4x4)) {}

        DirectX::XMMATRIX m_Matrix;
    };
	
    const XMMatrix4 XMMatrix4::Identity = XMMatrix4{ DirectX::XMMatrixIdentity() };
    
    inline XMVector4 operator*(XMVector4 v, const XMMatrix4& m) noexcept { return DirectX::XMVector4Transform(v, m); }




    template<typename Matrix>
    concept MatrixType = std::is_same_v<Matrix, XMMatrix3> || std::is_same_v<Matrix, XMMatrix4>;

    template<MatrixType Matrix>
    inline Matrix operator-(Matrix m) noexcept { return Matrix{} -= m; }
    template<MatrixType Matrix>
    inline Matrix operator*(Matrix m, XMScalar s) noexcept { return m *= s; }
    template<MatrixType Matrix>
    inline Matrix operator*(XMScalar s, const Matrix& rhs) noexcept { return rhs * s; }
    template<MatrixType Matrix>
    inline Matrix operator*(Matrix m, float s) noexcept { return m *= s; }
    template<MatrixType Matrix>
    inline Matrix operator*(float s, const Matrix& rhs) noexcept { return rhs * s; }
    template<MatrixType Matrix>
    inline Matrix operator/(Matrix m, XMScalar s) noexcept { return m /= s; }
    template<MatrixType Matrix>
    inline Matrix operator/(XMScalar s, const Matrix& rhs) noexcept { return rhs / s; }
    template<MatrixType Matrix>
    inline Matrix operator/(Matrix m, float s) noexcept { return m /= s; }
    template<MatrixType Matrix>
    inline Matrix operator/(float s, const Matrix& rhs) noexcept { return rhs / s; }
    template<MatrixType Matrix>
    inline Matrix operator+(Matrix m, XMScalar s) noexcept { return m += s; }
    template<MatrixType Matrix>
    inline Matrix operator+(XMScalar s, const Matrix& rhs) noexcept { return rhs + s; }
    template<MatrixType Matrix>
    inline Matrix operator+(Matrix m, float s) noexcept { return m += s; }
    template<MatrixType Matrix>
    inline Matrix operator+(float s, const Matrix& rhs) noexcept { return rhs + s; }
    template<MatrixType Matrix>
    inline Matrix operator-(Matrix m, XMScalar s) noexcept { return m -= s; }
    template<MatrixType Matrix>
    inline Matrix operator-(XMScalar s, const Matrix& rhs) noexcept { return rhs - s; }
    template<MatrixType Matrix>
    inline Matrix operator-(Matrix m, float s) noexcept { return m -= s; }
    template<MatrixType Matrix>
    inline Matrix operator-(float s, const Matrix& rhs) noexcept { return rhs - s; }
    template<MatrixType Matrix>
    inline Matrix operator+(Matrix lhs, const Matrix& rhs) noexcept { return lhs += rhs; }
    template<MatrixType Matrix>
    inline Matrix operator-(Matrix lhs, const Matrix& rhs) noexcept { return lhs -= rhs; }
    template<MatrixType Matrix>
    inline Matrix operator*(Matrix lhs, const Matrix& rhs) noexcept { return lhs *= rhs; }

}

#endif
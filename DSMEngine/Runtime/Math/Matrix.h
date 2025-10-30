#pragma once
#ifndef __MATRIX__H__
#define __MATRIX__H__

#include "Quaternion.h"

namespace DSM {
	template<size_t Row, size_t Col>
	concept Matrix3or4 = ((Row == 3 && Col == 3) || (Row == 4 && Col == 4));

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	class Matrix
	{
		friend class Matrix<T, Row + 1, Col + 1>;
	public:
		using RowType = Vector<T, Col>;
		using ColType = Vector<T, Row>;
		using LowerType = Matrix<T, Row - 1, Col - 1>;
		using UpperType = Matrix<T, Row + 1, Col + 1>;

        constexpr Matrix() = default;
        constexpr Matrix(const RowType& x, const RowType& y, const RowType& z) requires (Row == 3)
			:m_Matrix({x, y, z}) {}
        constexpr Matrix(const Vector<T, Col - 1>& x, const Vector<T, Col - 1>& y, 
			const Vector<T, Col - 1>& z, const Vector<T, Col - 1>& w) requires (Row == 4) 
			:m_Matrix({RowType{x}, RowType{y}, RowType{z}, RowType{w}}) {}
        constexpr Matrix(const RowType& x, const RowType& y, const RowType& z, const RowType& w) requires (Row == 4)
			:m_Matrix({x, y, z, w}) {}
        constexpr Matrix(Quaternion<T> q) requires Matrix3or4<Row, Col>;
        constexpr Matrix(const LowerType& m) requires (Row > 1 && Col > 1)
			:Matrix(m, Vector<T, Col - 1>{}) {}
        constexpr Matrix(const LowerType& m, const Vector<T, Col - 1>& w) requires (Row > 1 && Col > 1);
		explicit constexpr Matrix(const UpperType& m);

        auto& operator*=(Scalar<T> s) noexcept;
        inline auto& operator*=(T s) noexcept { return operator*=(Scalar<T>(s)); }
		auto& operator/=(Scalar<T> v) noexcept;
		inline auto& operator/=(T v) noexcept { return operator*=(Scalar<T>(v)); }
		auto& operator+=(const Matrix& other);
		auto& operator-=(const Matrix& other);

        inline bool operator==(const Matrix& m) const noexcept = default;

        inline RowType Get(size_t index) const noexcept{ return m_Matrix[index];}
        inline Scalar<T> Get(size_t row, size_t col) const noexcept { return Get(row).Get(col); }
        inline void Set(size_t index, RowType x) noexcept { m_Matrix[index] = std::move(x); }
        inline void Set(size_t row, size_t col, Scalar<T> val) noexcept { Get(row).Set(col, std::move(val)); }

        static inline Matrix GetRotate(Quaternion<T> q) noexcept requires Matrix3or4<Row, Col> { return Matrix{q}; }
        static Matrix GetRotateX(float angle) noexcept requires Matrix3or4<Row, Col>;
        static Matrix GetRotateY(float angle) noexcept requires Matrix3or4<Row, Col>;
        static Matrix GetRotateZ(float angle) noexcept requires Matrix3or4<Row, Col>;
        static inline Matrix GetScale(float s) noexcept requires Matrix3or4<Row, Col> { return GetScale(s, s, s); }
        static Matrix GetScale(float x, float y, float z) noexcept requires Matrix3or4<Row, Col>;
        static inline Matrix GetScale(const Vector<T, 3>& s) noexcept requires Matrix3or4<Row, Col> { return GetScale(s.Get(0), s.Get(1), s.Get(2)); }
        static inline Matrix Inverse(Matrix m) noexcept requires (Row == Col) { auto det = m.CalculateDet(); assert(det != 0); return m.Adjugate() / det; }
        static Matrix<T, Col, Row> Transpose(Matrix m) noexcept;
        static inline Matrix InverseTranspose(Matrix m) noexcept requires (Row == Col) { return Transpose(Inverse(m)); }

        static const Matrix Identity;

	private:
		// 获取去掉一行一列的子矩阵
		constexpr auto GetSubmatrix(std::size_t row, std::size_t col) const;
		constexpr T Cofactor(std::size_t row, std::size_t col) const;
		constexpr T CalculateDet() const;
		constexpr auto Adjugate() const requires (Row == Col);

		static inline Matrix _Identity() noexcept { Matrix ret{}; ret.Set(Row - 1, Col - 1, 1); return ret; }

	private:
		std::array<RowType, Row> m_Matrix{};
	};

    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    const Matrix<T, Row, Col> Matrix<T, Row, Col>::Identity = Matrix<T, Row, Col>::_Identity();
    

    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	constexpr Matrix<T, Row, Col>::Matrix(Quaternion<T> q) requires Matrix3or4<Row, Col>
    {
		Quaternion<T>::Normalize(q);
        
        T xx = q.Get(1) * q.Get(1);
        T yy = q.Get(2) * q.Get(2);
        T zz = q.Get(3) * q.Get(3);
        T xy = q.Get(1) * q.Get(2);
        T xz = q.Get(1) * q.Get(3);
        T yz = q.Get(2) * q.Get(3);
        T wx = q.Get(0) * q.Get(1);
        T wy = q.Get(0) * q.Get(2);
        T wz = q.Get(0) * q.Get(3);
        
        m_Matrix[0].Set(0, 1 - 2 * (yy + zz));
        m_Matrix[1].Set(0, 2 * (xy - wz));
        m_Matrix[2].Set(0, 2 * (xz + wy));
        
        m_Matrix[0].Set(1, 2 * (xy + wz));
        m_Matrix[1].Set(1, 1 - 2 * (xx + zz));
        m_Matrix[2].Set(1, 2 * (yz - wx));
        
        m_Matrix[0].Set(2, 2 * (xz - wy));
        m_Matrix[1].Set(2, 2 * (yz + wx));
        m_Matrix[2].Set(2, 1 - 2 * (xx + yy));

		if constexpr(Row == 4){
			m_Matrix[0].Set(3, 0);
			m_Matrix[1].Set(3, 0);
			m_Matrix[2].Set(3, 0);
			m_Matrix[3] = RowType{0, 0, 0, 1};
		}
    }

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	constexpr Matrix<T, Row, Col>::Matrix(const LowerType& m, const Vector<T, Col - 1>& w) requires (Row > 1 && Col > 1)
	{
		for(size_t i = 0; i < Row - 1; ++i){
			m_Matrix[i] = RowType{m.Get(i)};
		}
		m_Matrix[Row - 1] = RowType{w};
		m_Matrix[Row -1].Set(Col - 1, 1);
	}

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	constexpr Matrix<T, Row, Col>::Matrix(const UpperType& m)
	{
		for(size_t i = 0; i < Row; ++i){
			m_Matrix[i] = RowType{m.Get(i)};
		}
	}

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	auto& Matrix<T, Row, Col>::operator*=(Scalar<T> s) noexcept
	{
		m_Matrix[0] *= s;
		m_Matrix[1] *= s;
		m_Matrix[2] *= s;
		return *this;
	}

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	auto& Matrix<T, Row, Col>::operator/=(Scalar<T> v) noexcept
	{
		for (auto i = Row; i--; m_Matrix[i] /= v);
		return *this;
	}

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	auto& Matrix<T, Row, Col>::operator+=(const Matrix& other)
	{
		for (auto i = Row; i--; m_Matrix[i] += other.Get(i));
		return *this;
	}

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	auto& Matrix<T, Row, Col>::operator-=(const Matrix& other)
	{
		for (auto i = Row; i--; m_Matrix[i] -= other.Get(i));
		return *this;
	}

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	Matrix<T, Row, Col> Matrix<T, Row, Col>::GetRotateX(float angle) noexcept requires Matrix3or4<Row, Col>
	{
		T cos = std::cos(angle);
		T sin = std::sin(angle);

		Matrix<T, Row, Col> ret{};
		ret.Set(0, RowType{1, 0, 0});
		ret.Set(1, RowType{0, cos, sin});
		ret.Set(2, RowType{0, -sin, cos});
		if constexpr(Row == 4){
			ret.Set(4, RowType{0, 0, 0, 1});
		}
		return ret;
	}

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	Matrix<T, Row, Col> Matrix<T, Row, Col>::GetRotateY(float angle) noexcept requires Matrix3or4<Row, Col>
	{
		T cos = std::cos(angle);
		T sin = std::sin(angle);

		Matrix<T, Row, Col> ret{};
		ret.Set(0, RowType{cos, 0, -sin});
		ret.Set(1, RowType{0, 1, 0});
		ret.Set(2, RowType{sin, 0, cos});
		if constexpr(Row == 4){
			ret.Set(4, RowType{0, 0, 0, 1});
		}
		return ret;
	}
	
	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	Matrix<T, Row, Col> Matrix<T, Row, Col>::GetRotateZ(float angle) noexcept requires Matrix3or4<Row, Col>
	{
		T cos = std::cos(angle);
		T sin = std::sin(angle);

		Matrix<T, Row, Col> ret{};
		ret.Set(0, RowType{cos, sin, 0});
		ret.Set(1, RowType{-sin, cos, 0});
		ret.Set(2, RowType{0, 0, 0});
		if constexpr(Row == 4){
			ret.Set(3, RowType{0, 0, 0, 1});
		}
		return ret;
	}

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    Matrix<T, Row, Col> Matrix<T, Row, Col>::GetScale(float x, float y, float z) noexcept requires Matrix3or4<Row, Col>
    {
		Matrix<T, Row, Col> ret{};
		ret.Set(0, RowType{x, 0, 0});
		ret.Set(1, RowType{0, y, 0});
		ret.Set(2, RowType{0, 0, z});
		if constexpr(Row == 4){
			ret.Set(3, RowType{0, 0, 0, 1});
		}
		return ret;
    }

	// 计算当前矩阵去掉一行一列后的子矩阵
	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	constexpr auto Matrix<T, Row, Col>::GetSubmatrix(std::size_t row, std::size_t col) const
	{
		if (!(0 <= row && row < Row) || !(0 <= col && col < Col))
			throw std::logic_error("Row or col out of range.");
		
		LowerType ret{};
		for (auto i = Row - 1; i--; ) {
			for (auto j = Col - 1; j--; ) {
				ret.Set(i, j, Get(i < row ? i : i + 1, j < col ? j : j + 1));
			}
		}
		return ret;
	}

    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	Matrix<T, Col, Row> Matrix<T, Row, Col>::Transpose(Matrix m) noexcept
	{
		Matrix<T, Col, Row> ret{};
		for(size_t i = 0; i < Row; ++i){
			for(size_t j = 0; j < Col; ++j){
				ret.Set(j, i, m.Get(i, j));
			}
		}
		return m;
    }

    // 计算矩阵的代数余子式
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	constexpr T Matrix<T, Row, Col>::Cofactor(std::size_t row, std::size_t col) const
	{
		auto det = GetSubmatrix(row, col).CalculateDet();
		return det * ((row + col) % 2 ? -1 : 1);
	}

	// 计算当前矩阵的行列式
	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	constexpr T Matrix<T, Row, Col>::CalculateDet() const
	{
		T ret{};
		// 递归计算代数余子式，当行列为1时终止
		if constexpr(Row == 1 && Col == 1){
			ret =  Get(0, 0);
		}
		else{
			for(size_t i = Row; i--; ret += Get(0, i) * Cofactor(0, i));
		}
		return ret;
	}

	// 计算伴随矩阵
	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
	constexpr auto Matrix<T, Row, Col>::Adjugate() const requires (Row == Col)
	{
		Matrix ret{};
		for (auto i = Row; i--; )
			for (auto j = Col; j--; ret.Set(j, i, Cofactor(i, j)));
		return ret;
	}




	// 向量与行矩阵相乘
	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Vector<T, Col> operator*(const Vector<T, Row>& v, const Matrix<T, Row, Col>& m) noexcept
    {
		Vector<T, Col> ret{};
		for(size_t i = 0; i < Col; ++i){
			for(size_t j = 0; j < Row; ++j){
				ret.Set(i, ret.Get(i) + v.Get(j) * m.Get(j, i));
			}
		}
		return ret;
    }
    // 行矩阵相乘
	template <typename T, size_t L, size_t C, size_t R> requires std::is_arithmetic_v<T>
    inline auto operator*(const Matrix<T, L, C>& lhs, const Matrix<T, C, R>& rhs) noexcept
    {
		Matrix<T, L, R> ret{};
		for(size_t i = 0; i < L; ++i){
			ret.Set(i, lhs.Get(i) * rhs);
		}
		return ret;
    }

	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator-(const Matrix<T, Row, Col>& m) noexcept { return Matrix<T, Row, Col>{} - m; }
	template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator*(Matrix<T, Row, Col> m, Scalar<T> s) noexcept { return m *= s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator*(Scalar<T> s, const Matrix<T, Row, Col>& rhs) noexcept { return rhs * s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator*(Matrix<T, Row, Col> m, float s) noexcept { return m *= s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator*(float s, const Matrix<T, Row, Col>& rhs) noexcept { return rhs * s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator/(Matrix<T, Row, Col> m, Scalar<T> s) noexcept { return m /= s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator/(Scalar<T> s, const Matrix<T, Row, Col>& rhs) noexcept { return rhs / s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator/(Matrix<T, Row, Col> m, float s) noexcept { return m /= s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator/(float s, const Matrix<T, Row, Col>& rhs) noexcept { return rhs / s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator+(Matrix<T, Row, Col> m, Scalar<T> s) noexcept { return m += s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator+(Scalar<T> s, const Matrix<T, Row, Col>& rhs) noexcept { return rhs + s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator+(Matrix<T, Row, Col> m, float s) noexcept { return m += s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator+(float s, const Matrix<T, Row, Col>& rhs) noexcept { return rhs + s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator-(Matrix<T, Row, Col> m, Scalar<T> s) noexcept { return m -= s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator-(Scalar<T> s, const Matrix<T, Row, Col>& rhs) noexcept { return rhs - s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator-(Matrix<T, Row, Col> m, float s) noexcept { return m -= s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator-(float s, const Matrix<T, Row, Col>& rhs) noexcept { return rhs - s; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator+(Matrix<T, Row, Col> lhs, Matrix<T, Row, Col> rhs) noexcept { return lhs += rhs; }
    template <typename T, std::size_t Row, std::size_t Col> requires std::is_arithmetic_v<T>
    inline Matrix<T, Row, Col> operator-(Matrix<T, Row, Col> lhs, const Matrix<T, Row, Col>& rhs) noexcept { return lhs -= rhs; }


	using Matrix3f = Matrix<float, 3, 3>;
	using Matrix3d = Matrix<float, 3, 3>;
	using Matrix3i = Matrix<int, 3, 3>;
	using Matrix4f = Matrix<float, 4, 4>;
	using Matrix4d = Matrix<float, 4, 4>;
	using Matrix4i = Matrix<int, 4, 4>;
}


#endif // !__MATRIX__H__

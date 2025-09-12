#pragma once
#ifndef __GEOMETRY__H__
#define __GEOMETRY__H__

#include <vector>
#include "Runtime/Math/MathCommon.h"

namespace DSM {
	namespace Geometry {

		struct Vertex
		{
			Math::Vector3 position{0,0,0};
			Math::Vector3 normal{0,0,0};
			Math::Vector4 tangent{0,0,0,0};
			Math::Vector3 biTangent{0,0,0};
			Math::Vector2 texCoord{0,0};
		};

		struct GeometryMesh
		{
			std::vector<Vertex> vertices;
			std::vector<std::uint32_t> indices32;

			std::vector<std::uint16_t>& GetIndices16() noexcept {
				if (m_Indices16.empty() || m_Indices16.size() != indices32.size()) {
					m_Indices16.reserve(indices32.size());
					for (const auto& index : indices32) {
						m_Indices16.push_back(static_cast<std::uint16_t>(index));
					}
				}
				return m_Indices16;
			}

		private:
			std::vector<std::uint16_t> m_Indices16;
		};

		class GeometryGenerator
		{
		public:
			static GeometryMesh CreateBox(
				float width,
				float height,
				float depth,
				std::uint32_t subdivision) noexcept;

			/// <summary>
			/// 创建柱体
			/// </summary>
			/// <param name="topRadius"></param>	顶部半径
			/// <param name="buttonRadius"></param>	底部半径
			/// <param name="height"></param>		高度
			/// <param name="sliceCount"></param>	纵向细分程度，也就是底部的顶点数
			/// <param name="stackCount"></param>	横向细分程度，也就是柱体由几层构成
			/// <returns></returns>
			static GeometryMesh CreateCylinder(
				float topRadius,
				float buttonRadius,
				float height,
				std::uint32_t sliceCount,
				std::uint32_t stackCount) noexcept;

			/// <summary>
			/// 创建正多边形
			/// </summary>
			/// <param name="radius"></param>		// 多边形的半径
			/// <param name="sliceCount"></param>	// 多边形的边数
			/// <returns></returns>
			static GeometryMesh CreatePolygon(
				float radius,
				std::uint32_t sliceCount) noexcept;

			static GeometryMesh CreateSphere(
				float radius,
				std::uint32_t sliceCount,
				std::uint32_t stackCount) noexcept;

			static GeometryMesh CreateGeosphere(
				float radius,
				std::uint32_t subdivision) noexcept;

			static GeometryMesh CreateGrid(
				float width,
				float depth,
				std::uint32_t m,
				std::uint32_t n) noexcept;


			template<typename VertFunc, typename IndexFunc>
			static GeometryMesh MergeMesh(
				const GeometryMesh& m0,
				const GeometryMesh& m1,
				VertFunc vertFunc,
				IndexFunc indexFunc) noexcept;
			static GeometryMesh MergeMesh(
				const GeometryMesh& m0,
				const GeometryMesh& m1) noexcept;

		private:
			// 将网格按三角形细分
			static void Subdivide(GeometryMesh& mesh) noexcept;
			static Vertex MidPoint(const Vertex& v0, const Vertex& v1) noexcept;
		};

	}


}

#endif
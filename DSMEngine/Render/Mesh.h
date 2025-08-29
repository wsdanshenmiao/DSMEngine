#pragma once
#ifndef __MESH_H__
#define __MESH_H__

#include <map>
#include <string>
#include "Graphics/PipelineState.h"
#include "Graphics/ResourceBindings.h"

namespace DSM {
	struct Material;

	enum PSOFlags : uint16_t
	{
		kHasPosition = ( 1 << 0 ),
		kHasNormal = ( 1 << 1 ),
		kHasTangent = ( 1 << 2 ),
		kHasUV = ( 1 << 3 ),
		kAlphaBlend = ( 1 << 4 ),
		kAlphaTest = ( 1 << 5 ),
		kBothSide = ( 1 << 6 ),
	};

	struct Mesh
	{
		std::string name;
		
		// 设置顶点缓冲区使用的数据
		VertexBufferBinding positionStream;
		VertexBufferBinding normalStream;
		VertexBufferBinding uvStream;
		VertexBufferBinding tangentStream;
		// 索引缓冲区使用的数据
		IndexBufferBinding indexBufferViews;
		uint16_t psoFlags;
		uint16_t psoIndex;

		// 每次绘制需要使用的数据
		struct SubMesh
		{
			uint32_t indexCount;
			uint32_t indexOffset;
			uint32_t vertexOffset;
			uint16_t materialIndex;
			// 使用的纹理在描述符堆中的偏移
			std::vector<TextureHandle> textures;
		};
		std::map<std::string, SubMesh> subMeshes;

		BufferHandle meshData{};
	};

}

#endif

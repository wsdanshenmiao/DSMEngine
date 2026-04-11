#pragma once
#ifndef __MESH_H__
#define __MESH_H__

#include <string>
#include "Runtime/Core/Macro.h"
#include "Runtime/Graphics/Device.h"
#include "Runtime/Graphics/CommandList.h"
#include "Runtime/Math/Collision/BoundingBox.h"

namespace DSM {

	struct Mesh
	{
		enum VertexAttributeSlot
		{
			Position = 0,
			UV,
			Normal,
			Tangent,
			Count
		};

		struct SubMesh
		{
			Math::AxisAlignedBox bounds{};
			PrimitiveType primitiveType{};
			size_t indexCount{};
			size_t indexOffset{};
			size_t vertexOffset{};
		};

		std::string name;
		Format indexFormat = Format::R32_UINT;
		std::vector<uint8_t> indices{};
		
		size_t vertexBufferCount{};
		Math::AxisAlignedBox bounds{};

		std::vector<Math::Vector3> vertices{};
		std::vector<Math::Vector3> normals{};
		std::vector<Math::Vector4> tangents{};
		std::vector<Math::Vector2> uv{};

		inline static std::array<VertexAttributeDesc, VertexAttributeSlot::Count> sm_VertexAttributeNames = {
			VertexAttributeDesc{}.SetName("POSITION").SetBufferIndex(Position)
				.SetFormat(Format::RGB32_FLOAT).SetElementStride(sizeof(Math::Vector3)),
			VertexAttributeDesc{}.SetName("TEXCOORD").SetBufferIndex(UV)
				.SetFormat(Format::RG32_FLOAT).SetElementStride(sizeof(Math::Vector2)),
			VertexAttributeDesc{}.SetName("NORMAL").SetBufferIndex(Normal)
				.SetFormat(Format::RGB32_FLOAT).SetElementStride(sizeof(Math::Vector3)),
			VertexAttributeDesc{}.SetName("TANGENT").SetBufferIndex(Tangent)
				.SetFormat(Format::RGB32_FLOAT).SetElementStride(sizeof(Math::Vector4)),
		};

		void Clear() noexcept;

		inline const std::string& GetName() const noexcept { return name; }

		BufferHandle GetIndexBuffer() const noexcept;
		size_t GetIndexCount(size_t subMeshIndex) const;
		size_t GetIndexOffset(size_t subMeshIndex) const;
    	template <typename T> requires std::same_as<T, uint16_t> || std::same_as<T, uint32_t>
		std::span<T> GetIndices(size_t subMeshIndex) const;

		IndexBufferBinding GetIndexBufferBinding(size_t subMeshIndex) const noexcept;

		BufferHandle GetVertexBuffer() const noexcept;
		std::span<const Math::Vector3> GetVertices() const noexcept;
		std::span<const Math::Vector3> GetNormals() const noexcept;
		std::span<const Math::Vector4> GetTangents() const noexcept;
		std::span<const Math::Vector2> GetUVs() const noexcept;
		size_t GetVertexOffset(size_t subMeshIndex) const;

		VertexBufferBinding GetVertexBufferBinding(VertexAttributeSlot slot) const noexcept;

		VertexAttributeDesc GetVertexAttribute(VertexAttributeSlot slot) const noexcept;
		const auto& GetVertexAttributes() const noexcept;
		bool HasVertexAttribute(VertexAttributeSlot slot) const noexcept;

		PrimitiveType GetPrimitiveType(size_t subMeshIndex) const noexcept;

		inline size_t GetSubMeshCount() const noexcept { return m_SubMeshes.size(); }
		SubMesh GetSubMesh(size_t subMeshIndex) const noexcept;
		const std::vector<SubMesh>& GetSubMeshes() const noexcept;

		inline Mesh& SetName(const std::string& value) { name = value; return *this; }

		inline Mesh& SetIndexFormat(Format format) noexcept { indexFormat = format; return *this; }
    	template <typename T> requires std::same_as<T, uint16_t> || std::same_as<T, uint32_t>
		Mesh& SetIndexBufferData(std::span<const T> data, size_t bufferOffset);
    	template <typename T> requires std::same_as<T, uint16_t> || std::same_as<T, uint32_t>
		Mesh& SetIndices(
			std::span<const T> _indices,
			PrimitiveType primitiveType, 
			size_t subMeshIndex, 
			Math::AxisAlignedBox subMeshBounds = {},
			size_t vertexOffset = 0);

		template<typename T>
		Mesh& SetVertexBufferData(std::span<const T> _vertices, size_t bufferOffset, VertexAttributeSlot slot);
		Mesh& SetVertices(std::vector<Math::Vector3> _vertices);
		Mesh& SetNormals(std::vector<Math::Vector3> _normals);
		Mesh& SetTangents(std::vector<Math::Vector4> _tangents);
		Mesh& SetUVs(std::vector<Math::Vector2> _uv);

		Mesh& SetSubMesh(size_t subMeshIndex, const SubMesh& subMesh);
		Mesh& SetSubMeshes(std::span<const SubMesh> subMeshes);

		void UploadBuffer();

		static void Create(IDevice* device);
		static void Destroy();

	private:
		template<typename T>
    	void CheckIndexFormat(Format indexFormat) const;

	private:
		inline static IDevice* sm_Device;

		BufferHandle m_VertexBuffer{};
		BufferHandle m_IndexBuffer{};
		std::vector<SubMesh> m_SubMeshes{};
	};

    template <typename T> requires std::same_as<T, uint16_t> || std::same_as<T, uint32_t>
    inline std::span<T> Mesh::GetIndices(size_t subMeshIndex) const
    {
		if(subMeshIndex >= m_SubMeshes.size()){
			DSM_CORE_ASSERT(subMeshIndex < m_SubMeshes.size(), "subMeshIndex out of range");
			return {};
		}
		CheckIndexFormat<T>(indexFormat);

		size_t stride = (indexFormat == Format::R16_UINT) ? sizeof(uint16_t) : sizeof(uint32_t);
		size_t indexSize = GetIndexCount(subMeshIndex) * stride;
		uint8_t* indexData = indices[GetIndexOffset(subMeshIndex) * stride].data();
        return std::span<T>(indexData, indexSize / sizeof(T));
    }

    template <typename T> requires std::same_as<T, uint16_t> || std::same_as<T, uint32_t>
    inline Mesh &Mesh::SetIndexBufferData(std::span<const T> _indices, size_t bufferOffset)
    {
		if(_indices.empty())
			return *this;
		
		CheckIndexFormat<T>(indexFormat);
		
		DSM_CORE_ASSERT(sm_Device != nullptr, "Device not initialized. Call Mesh::Create() first.");
		CommandListHandle cmdList = sm_Device->CreateCommandList(
			CommandListParameters{}.SetDebugName(name + "_SetIndexBufferData"));
		cmdList->Open();

		size_t byteSize = _indices.size() * sizeof(T);
		if(m_IndexBuffer == nullptr || m_IndexBuffer->GetDesc().byteSize < (byteSize + bufferOffset)){
			auto preBuffer = m_IndexBuffer;
			m_IndexBuffer = sm_Device->CreateBuffer(BufferDesc{}
				.SetByteSize(byteSize)
				.SetIsIndexBuffer(true)
				.SetFormat(indexFormat)
				.SetStructStride(sizeof(T))
				.SetDebugName(name + "_IndexBuffer"));
			if(preBuffer != nullptr) {
				cmdList->CopyBuffer(m_IndexBuffer, 0, preBuffer, 0, std::min(byteSize, preBuffer->GetDesc().byteSize));
			}
		}

		cmdList->WriteBuffer(m_IndexBuffer, _indices.data(), byteSize, bufferOffset);
		cmdList->Close();
		sm_Device->ExecuteCommandList(cmdList);

		return *this;
    }

    template <typename T> requires std::same_as<T, uint16_t> || std::same_as<T, uint32_t>
    inline Mesh &Mesh::SetIndices(
		std::span<const T> _indices, 
		PrimitiveType primitiveType, 
		size_t subMeshIndex, 
		Math::AxisAlignedBox subMeshBounds,
		size_t vertexOffset)
	{
		if(_indices.empty()) {
			return *this;
		}

		CheckIndexFormat<T>(indexFormat);

		SubMesh subMesh{};
		subMesh.primitiveType = primitiveType;
		subMesh.indexCount = _indices.size();
		subMesh.vertexOffset = vertexOffset;
		if(subMeshBounds.IsValid()) {
			subMesh.bounds = subMeshBounds;
		}
		else{
			// Compute bounds from indices when explicit bounds are not provided.
			Math::Vector3 min(std::numeric_limits<float>::max());
			Math::Vector3 max(std::numeric_limits<float>::lowest());
			for(size_t i = 0; i < _indices.size(); ++i) {
				size_t vertexIndex = static_cast<size_t>(_indices[i]) + vertexOffset;
				if(vertexIndex < vertices.size()) {
					const auto& vertex = vertices[vertexIndex];
					min = Math::Vector3::Min(min, vertex);
					max = Math::Vector3::Max(max, vertex);
				}
			}
			subMesh.bounds = Math::AxisAlignedBox(min, max);
		}
		bounds = Math::AxisAlignedBox::Union(bounds, subMesh.bounds);

		if(subMeshIndex >= m_SubMeshes.size()) {
			subMesh.indexOffset = indices.size() / sizeof(T);
			m_SubMeshes.resize(subMeshIndex + 1);
		}
		else{
			// Adjust following submesh offsets when inserting into the middle.
			if(subMeshIndex > 0){
				subMesh.indexOffset = m_SubMeshes[subMeshIndex - 1].indexOffset + m_SubMeshes[subMeshIndex - 1].indexCount;
			}
			for(size_t i = subMeshIndex + 1; i < m_SubMeshes.size(); ++i) {
				m_SubMeshes[i].indexOffset = m_SubMeshes[i - 1].indexOffset + m_SubMeshes[i - 1].indexCount;
			}
		}
		m_SubMeshes[subMeshIndex] = subMesh;

		if(auto indexSize = (subMesh.indexOffset + subMesh.indexCount) * sizeof(T); indices.size() < indexSize) {
			indices.resize(indexSize);
		}
		std::memcpy(indices.data() + subMesh.indexOffset * sizeof(T), _indices.data(), subMesh.indexCount * sizeof(T));
		return *this;
    }

    template <typename T>
    inline Mesh &Mesh::SetVertexBufferData(std::span<const T> vertices, size_t bufferOffset, VertexAttributeSlot slot)
    {
		if(vertices.empty() || slot >= VertexAttributeSlot::Count)
			return *this;

		DSM_CORE_ASSERT(sm_Device != nullptr, "Device not initialized. Call Mesh::Create() first.");
		CommandListHandle cmdList = sm_Device->CreateCommandList(
			CommandListParameters{}.SetDebugName(name + "_SetVertexBufferData"));
		cmdList->Open();

		size_t byteSize = vertices.size() * sizeof(T);
		if(m_VertexBuffer == nullptr || m_VertexBuffer->GetDesc().byteSize < (byteSize + bufferOffset)){
			auto preBuffer = m_VertexBuffer;
			m_VertexBuffer = sm_Device->CreateBuffer(BufferDesc{}
				.SetByteSize(byteSize + bufferOffset)
				.SetDebugName(name + "_VertexBuffer"));
			if(preBuffer != nullptr) {
				cmdList->CopyBuffer(m_VertexBuffer, 0, preBuffer, 0, std::min(byteSize + bufferOffset, preBuffer->GetDesc().byteSize));
			}
		}

		cmdList->WriteBuffer(m_VertexBuffer, vertices.data(), byteSize, bufferOffset);
		cmdList->Close();
		sm_Device->ExecuteCommandList(cmdList);

		return *this;
	}
    
	template <typename T>
    inline void Mesh::CheckIndexFormat(Format indexFormat) const
    {
		if constexpr(std::same_as<T, uint16_t>) {
			DSM_CORE_ASSERT(indexFormat == Format::R16_UINT, "Index format mismatch. Expected R16_UINT.");
		}
		else if constexpr(std::same_as<T, uint32_t>) {
			DSM_CORE_ASSERT(indexFormat == Format::R32_UINT, "Index format mismatch. Expected R32_UINT.");
		}
	}
}

#endif


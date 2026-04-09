#include "Runtime/Render/Mesh.h"
#include "Mesh.h"

namespace DSM {
	void Mesh::Clear() noexcept
	{
		indexFormat = Format::UNKNOWN;
		vertexBufferCount = 0;
		bounds = {};
		vertices.clear();
		normals.clear();
		tangents.clear();
		uv.clear();

		m_SubMeshes.clear();
		m_VertexBuffer = nullptr;
		m_IndexBuffer = nullptr;
	}

	size_t Mesh::GetBaseVertex(size_t subMeshIndex) const
	{
		if (subMeshIndex >= m_SubMeshes.size()) {
			DSM_CORE_ASSERT(subMeshIndex < m_SubMeshes.size(), "subMeshIndex out of range");
			return 0;
		}
		return m_SubMeshes[subMeshIndex].vertexOffset;
	}

	BufferHandle Mesh::GetIndexBuffer() const noexcept
	{
		return m_IndexBuffer;
	}

	size_t Mesh::GetIndexCount(size_t subMeshIndex) const
	{
		if (subMeshIndex >= m_SubMeshes.size()) {
			DSM_CORE_ASSERT(subMeshIndex < m_SubMeshes.size(), "subMeshIndex out of range");
			return 0;
		}
		return m_SubMeshes[subMeshIndex].indexCount;
	}

	size_t Mesh::GetIndexOffset(size_t subMeshIndex) const
	{
		if (subMeshIndex >= m_SubMeshes.size()) {
			DSM_CORE_ASSERT(subMeshIndex < m_SubMeshes.size(), "subMeshIndex out of range");
			return 0;
		}
		return m_SubMeshes[subMeshIndex].indexOffset;
	}

    IndexBufferBinding Mesh::GetIndexBufferBinding(size_t subMeshIndex) const noexcept
    {
		if(subMeshIndex >= m_SubMeshes.size()) {
			DSM_CORE_ASSERT(subMeshIndex < m_SubMeshes.size(), "subMeshIndex out of range");
			return IndexBufferBinding{};
		}
		size_t indexStr = (indexFormat == Format::R16_UINT) ? sizeof(uint16_t) : sizeof(uint32_t);
		return IndexBufferBinding{}
			.SetBuffer(m_IndexBuffer)
			.SetFormat(indexFormat)
			.SetOffset(GetIndexOffset(subMeshIndex) * indexStr);
    }

    BufferHandle Mesh::GetVertexBuffer() const noexcept
    {
        return m_VertexBuffer;
    }

    std::span<const Math::Vector3> Mesh::GetVertices() const noexcept
	{
		return vertices;
	}

	std::span<const Math::Vector3> Mesh::GetNormals() const noexcept
	{
		return normals;
	}

	std::span<const Math::Vector4> Mesh::GetTangents() const noexcept
	{
		return tangents;
	}

	std::span<const Math::Vector2> Mesh::GetUVs() const noexcept
	{
		return uv;
	}

    size_t Mesh::GetVertexOffset(size_t subMeshIndex) const
    {
		if (subMeshIndex >= m_SubMeshes.size()) {
			DSM_CORE_ASSERT(subMeshIndex < m_SubMeshes.size(), "subMeshIndex out of range");
			return 0;
		}
        return m_SubMeshes[subMeshIndex].vertexOffset;
    }

    VertexBufferBinding Mesh::GetVertexBufferBinding(VertexAttributeSlot slot) const noexcept
    {
		size_t offset = 0;
		if(slot > VertexAttributeSlot::Position) {
			offset += vertices.size() * sizeof(Math::Vector3);
		}
		if(slot > VertexAttributeSlot::UV) {
			offset += uv.size() * sizeof(Math::Vector2);
		}
		if(slot > VertexAttributeSlot::Normal) {
			offset += normals.size() * sizeof(Math::Vector3);
		}
        return VertexBufferBinding{m_VertexBuffer, (uint32_t)slot, offset};
    }

    VertexAttributeDesc Mesh::GetVertexAttribute(VertexAttributeSlot slot) const noexcept
    {
		return sm_VertexAttributeNames[slot];
    }

    const auto& Mesh::GetVertexAttributes() const noexcept
    {
        return sm_VertexAttributeNames;
    }

    bool Mesh::HasVertexAttribute(VertexAttributeSlot slot) const noexcept
    {
        switch (slot) {
		case VertexAttributeSlot::Position: return !vertices.empty(); break;
		case VertexAttributeSlot::UV: return !uv.empty(); break;
		case VertexAttributeSlot::Normal: return !normals.empty(); break;
		case VertexAttributeSlot::Tangent: return !tangents.empty(); break;
		default:
			break;
		}
		return false;
    }

    PrimitiveType Mesh::GetPrimitiveType(size_t subMeshIndex) const noexcept
	{
		if (subMeshIndex >= m_SubMeshes.size()) {
			DSM_CORE_ASSERT(subMeshIndex < m_SubMeshes.size(), "subMeshIndex out of range");
			return PrimitiveType{};
		}
		return m_SubMeshes[subMeshIndex].primitiveType;
	}

	Mesh::SubMesh Mesh::GetSubMesh(size_t subMeshIndex) const noexcept
	{
		if (subMeshIndex >= m_SubMeshes.size()) {
			DSM_CORE_ASSERT(subMeshIndex < m_SubMeshes.size(), "subMeshIndex out of range");
			return SubMesh{};
		}
		return m_SubMeshes[subMeshIndex];
	}

	const std::vector<Mesh::SubMesh>& Mesh::GetSubMeshes() const noexcept
	{
		return m_SubMeshes;
	}

    Mesh &Mesh::SetVertices(std::vector<Math::Vector3> _vertices)
    {
		if(_vertices.empty()){
			if(!vertices.empty()){
				vertexBufferCount--;
			}
		}
		else if(vertices.empty()){
			vertexBufferCount++;
		}
		vertices = std::move(_vertices);
		return *this;
    }

    Mesh &Mesh::SetNormals(std::vector<Math::Vector3> _normals)
    {
		normals = std::move(_normals);
		return *this;
    }

	Mesh &Mesh::SetTangents(std::vector<Math::Vector4> _tangents)
	{
		tangents = std::move(_tangents);
		return *this;
	}

	Mesh &Mesh::SetUVs(std::vector<Math::Vector2> _uv)
	{
		uv = std::move(_uv);
		return *this;
	}

    Mesh &Mesh::SetSubMesh(size_t subMeshIndex, const SubMesh &subMesh)
    {
		if(subMeshIndex >= m_SubMeshes.size()) {
			m_SubMeshes.resize(subMeshIndex + 1);
		}

		m_SubMeshes[subMeshIndex] = subMesh;
		return *this;
    }

    Mesh &Mesh::SetSubMeshes(std::span<const SubMesh> subMeshes)
    {
		m_SubMeshes = std::vector<SubMesh>(subMeshes.begin(), subMeshes.end());
		return *this;
    }

    void Mesh::UploadBuffer()
    {
		if(indexFormat == Format::R16_FLOAT){
			SetIndexBufferData<uint16_t>(std::span<const uint16_t>((uint16_t*)indices.data(), indices.size() / sizeof(uint16_t)), 0);
		}
		else{
			SetIndexBufferData<uint32_t>(std::span<const uint32_t>((uint32_t*)indices.data(), indices.size() / sizeof(uint32_t)), 0);
		}
		size_t vertexOffset = 0;
		SetVertexBufferData<Math::Vector3>(vertices, vertexOffset, VertexAttributeSlot::Position);
		vertexOffset += vertices.size() * sizeof(Math::Vector3);
		SetVertexBufferData<Math::Vector2>(uv, vertexOffset, VertexAttributeSlot::UV);
		vertexOffset += uv.size() * sizeof(Math::Vector2);
		SetVertexBufferData<Math::Vector3>(normals, vertexOffset, VertexAttributeSlot::Normal);
		vertexOffset += normals.size() * sizeof(Math::Vector3);
		SetVertexBufferData<Math::Vector4>(tangents, vertexOffset, VertexAttributeSlot::Tangent);
	}

    void Mesh::Create(IDevice *device)
    {
		DSM_CORE_ASSERT(device != nullptr, "device cannot be null");
		sm_Device = device;
	}

    void Mesh::Destroy()
    {
		sm_Device = nullptr;
    }
}

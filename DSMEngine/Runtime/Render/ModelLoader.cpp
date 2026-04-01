#include "ModelLoader.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "Renderer/Renderer.h"
#include "Geometry.h"
#include "Runtime/Math/MathCommon.h"
#include "Runtime/Graphics/CommandList.h"
#include "Runtime/Graphics/Device.h"
#include "Runtime/Math/Collision/BoundingBox.h"
#include "TextureManager.h"
#include <filesystem>
#include <fstream>



namespace DSM::ModelLoader {

	static DeviceHandle s_GraphicsDevice;
	static std::array<TextureHandle, ShaderResource::kNumTextures> s_CommonTextures;

	struct MeshData
	{
		std::string name{};
		std::vector<Math::Vector3> positions{};
		std::vector<Math::Vector3> normals{};
		std::vector<Math::Vector2> texcoords{};
		std::vector<Math::Vector4> tangents{};
		std::vector<Math::Vector3> bitangents{};
		std::vector<uint32_t> indices{};
		Math::AxisAlignedBox boundingBox{};
		uint32_t materialIndex = 0;
		uint16_t psoFlags = 0;
	};


    void Init(IDevice *device)
    {
        s_GraphicsDevice = device;
		s_CommonTextures = {
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kBlackTransparent2D),
			TextureManager::GetDefaultTexture(TextureManager::kDefaultNormalTex)
		};
    }


    void Destroy()
	{
		s_GraphicsDevice = nullptr;
		s_CommonTextures.fill(nullptr);
	}

	
    void ProcessNode(Model& model, aiNode* node, const aiScene* scene);
    void ProcessMaterial(Model& model,const std::string& filename,const aiScene* scene);
    MeshData ProcessMesh(aiMesh* mesh);
    std::vector<std::shared_ptr<Mesh>> CreateMesh(const std::span<MeshData>& meshDatas);

	std::shared_ptr<Model> LoadModelFromGeometry(
		const std::string& name,
		const Geometry::GeometryMesh& geometryMesh,
		std::shared_ptr<ShaderResource::MaterialData> material)
	{
		if (geometryMesh.vertices.empty()){
			return nullptr;
		}

		auto model = std::make_shared<Model>();
		model->name = name;
		if(material == nullptr){
			auto mat = std::make_shared<ShaderResource::MaterialData>();
			mat->baseColor = Math::Vector4{1, 1, 1, 1};
			mat->emissiveColor = Math::Vector4{0, 0, 0, 0};
			mat->normalTexScale = 1;
			mat->metallicFactor = 1;
			mat->roughnessFactor = 1;
			model->materials.emplace_back(mat);
		}
		else{
			model->materials.push_back(material);
		}

		MeshData meshData{};
		meshData.indices = geometryMesh.indices32;
		meshData.name = name;
		meshData.materialIndex = 0;
		meshData.psoFlags |= kHasPosition | kHasNormal | kHasTangent | kHasUV;
		Math::Vector3 minVertex = geometryMesh.vertices[0].position;
		Math::Vector3 maxVertex = geometryMesh.vertices[0].position;
		for (const auto& vertex : geometryMesh.vertices) {
			meshData.positions.push_back(vertex.position);
			meshData.normals.push_back(vertex.normal);
			meshData.texcoords.push_back(vertex.texCoord);
			meshData.tangents.push_back(vertex.tangent);
			meshData.bitangents.push_back(vertex.biTangent);
			
			minVertex.Set(0, std::min(minVertex.Get(0), vertex.position.Get(0)));
			minVertex.Set(1, std::min(minVertex.Get(1), vertex.position.Get(1)));
			minVertex.Set(2, std::min(minVertex.Get(2), vertex.position.Get(2)));
			maxVertex.Set(0, std::max(maxVertex.Get(0), vertex.position.Get(0)));
			maxVertex.Set(1, std::max(maxVertex.Get(1), vertex.position.Get(1)));
			maxVertex.Set(2, std::max(maxVertex.Get(2), vertex.position.Get(2)));
		}
		meshData.boundingBox = Math::AxisAlignedBox(minVertex, maxVertex);

		model->meshes = CreateMesh({&meshData, 1});

		for(const auto& mesh : model->meshes){
			mesh->textures = std::vector<TextureHandle>{s_CommonTextures.begin(), s_CommonTextures.end()};
			model->boundingBox = Math::AxisAlignedBox::Union(model->boundingBox, mesh->boundingBox);
		}

		return model;
	}

    std::shared_ptr<Model> LoadModel(const std::string &filename)
    {
		std::shared_ptr<Model> model = nullptr;

		if(model == nullptr) {
			Assimp::Importer importer;
			const aiScene* pScene = importer.ReadFile(
				filename,
				aiProcess_ConvertToLeftHanded |     // 转为左手系
				aiProcess_GenBoundingBoxes |        // 获取碰撞盒
				aiProcess_Triangulate |             // 将多边形拆分
				aiProcess_ImproveCacheLocality |    // 改善缓存局部性
				aiProcess_SortByPType);             // 按图元顶点数排序用于移除非三角形图元

			if (nullptr == pScene || !pScene->HasMeshes()) {
				std::string warning = "[Warning]: Failed to load \"";
				warning += filename;
				warning += "\"\n";
				DSM_CORE_WARN(warning);
				return nullptr;
			}
			model = std::make_shared<Model>();
			model->name = pScene->mRootNode->mName.C_Str();

			ProcessNode(*model, pScene->mRootNode, pScene);
			ProcessMaterial(*model, filename, pScene);

			Math::AxisAlignedBox boundingBox{};
			for (const auto& mesh : model->meshes) {
				boundingBox = Math::AxisAlignedBox::Union(boundingBox, mesh->boundingBox);
			}
			model->boundingBox = boundingBox;
		}
		model->filePath = filename;

		return model;
	}

	void ProcessNode(Model& model, aiNode* node, const aiScene* scene)
	{
		// 导入当前节点的网格
		std::vector<MeshData> meshDatas{};
		meshDatas.reserve(node->mNumMeshes);
		for (UINT i = 0; i < node->mNumMeshes; ++i) {
			meshDatas.emplace_back(ProcessMesh(scene->mMeshes[node->mMeshes[i]]));
		}

		if (!meshDatas.empty()) {
			auto meshes = CreateMesh(meshDatas);
			model.meshes.append_range(meshes);
		}

		// 导入子节点的网格
		for (UINT i = 0; i < node->mNumChildren; ++i) {
			ProcessNode(model, node->mChildren[i], scene);
		}
	}

	MeshData ProcessMesh(aiMesh* mesh)
	{
		MeshData meshData{};
		meshData.name = mesh->mName.C_Str();

		// 获取顶点数据
		DSM_CORE_ASSERT(mesh->HasPositions());
		meshData.positions.resize(mesh->mNumVertices);
		meshData.psoFlags |= kHasPosition;
		if (mesh->HasNormals()) {
			meshData.normals.resize(mesh->mNumVertices);
			meshData.psoFlags |= kHasNormal;
		}
		if (mesh->HasTextureCoords(0)) {
			meshData.texcoords.resize(mesh->mNumVertices);
			meshData.psoFlags |= kHasUV;
		}
		if (mesh->HasTangentsAndBitangents()) {
			meshData.tangents.resize(mesh->mNumVertices);
			meshData.bitangents.resize(mesh->mNumVertices);
			meshData.psoFlags |= kHasTangent;
		}
		for (UINT i = 0; i < mesh->mNumVertices; ++i) {
			meshData.positions[i] = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
			if (!meshData.normals.empty()) {
				meshData.normals[i] = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
			}
			if (!meshData.texcoords.empty()) {
				meshData.texcoords[i] = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
			}
			if (!meshData.tangents.empty()) {
				meshData.tangents[i] = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z, 1.0f};
				meshData.bitangents[i] = {mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z};
			}
		}

		// 获取索引
		auto numIndex = mesh->mFaces->mNumIndices;
		meshData.indices.resize(mesh->mNumFaces * numIndex);
		for (size_t i = 0; i < mesh->mNumFaces; ++i) {
			memcpy(meshData.indices.data() + i * numIndex, mesh->mFaces[i].mIndices, sizeof(uint32_t) * numIndex);
		}

		const auto& aabb = mesh->mAABB;
		Math::Vector3 boxMin{aabb.mMin.x, aabb.mMin.y, aabb.mMin.z};
		Math::Vector3 boxMax{aabb.mMax.x, aabb.mMax.y, aabb.mMax.z};
		meshData.boundingBox = Math::AxisAlignedBox{boxMin, boxMax};

		meshData.materialIndex = mesh->mMaterialIndex;

		return meshData;
	}

	std::vector<std::shared_ptr<Mesh>> CreateMesh(const std::span<MeshData>& meshDatas)
	{
		if (meshDatas.empty()) 
			return {};

		std::string meshName = meshDatas.empty() ? "" : meshDatas[0].name;
		
		std::vector<Math::Vector3> positions{};
		std::vector<Math::Vector3> normals{};
		std::vector<Math::Vector2> uvs{};
		std::vector<Math::Vector4> tangents{};
		std::vector<uint32_t> indices{};
;
		UINT preIndexCount = 0;
		UINT preVertexCount = 0;
		std::vector<std::shared_ptr<Mesh>> meshes;
		for (const auto& meshData : meshDatas) {
			Mesh mesh{};
			auto indexCount = meshData.indices.size();
			mesh.materialIndex = meshData.materialIndex;
			mesh.indexCount = indexCount;
			mesh.indexOffset = preIndexCount;
			mesh.vertexOffset = preVertexCount;
			mesh.name = meshData.name;
			mesh.boundingBox = meshData.boundingBox;

			preIndexCount += indexCount;
			preVertexCount += meshData.positions.size();

			std::uint16_t posNormalFlags = kHasPosition;
			DSM_CORE_ASSERT((meshData.psoFlags & posNormalFlags) != 0);
			mesh.psoFlags = 0xffff;
			mesh.psoFlags &= meshData.psoFlags;
			positions.insert(positions.end(), meshData.positions.begin(), meshData.positions.end());
			normals.insert(normals.end(), meshData.normals.begin(), meshData.normals.end());
			uvs.insert(uvs.end(), meshData.texcoords.begin(), meshData.texcoords.end());
			tangents.insert(tangents.end(), meshData.tangents.begin(), meshData.tangents.end());
			indices.insert(indices.end(), meshData.indices.begin(), meshData.indices.end());

			meshes.push_back(std::make_shared<Mesh>(std::move(mesh)));
		}

		std::uint32_t posByteSize = positions.size() * sizeof(Math::Vector3);
		std::uint32_t normalByteSize = normals.size() * sizeof(Math::Vector3);
		std::uint32_t uvsByteSize = uvs.size() * sizeof(Math::Vector2);
		std::uint32_t tangentsByteSize = tangents.size() * sizeof(Math::Vector4);
		std::uint32_t indexByteSize = indices.size() * sizeof(std::uint32_t);

		assert(s_GraphicsDevice != nullptr);
		auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("CreateMesh"));
		cmdList->Open();

		auto meshDataBufferSize = posByteSize + normalByteSize + uvsByteSize + tangentsByteSize + indexByteSize;
		auto meshData = s_GraphicsDevice->CreateBuffer(
			BufferDesc().SetByteSize(meshDataBufferSize).SetDebugName("MeshData" + meshName));
		
		std::uint32_t offset = 0;
		cmdList->WriteBuffer(meshData, positions.data(), posByteSize, offset);
		auto positionStream = VertexBufferBinding().
			SetBuffer(meshData).SetOffset(offset).SetSlot(VertexAttributeSlot::Position);
		offset += posByteSize;

		VertexBufferBinding normalStream{};
		if (normals.size() > 0) {
			cmdList->WriteBuffer(meshData, normals.data(), normalByteSize, offset);
			normalStream = VertexBufferBinding().
				SetBuffer(meshData).SetOffset(offset).SetSlot(VertexAttributeSlot::Normal);
			offset += normalByteSize;
		}
		VertexBufferBinding uvStream{};
		if (uvs.size() > 0) {
			cmdList->WriteBuffer(meshData, uvs.data(), uvsByteSize, offset);
			uvStream = VertexBufferBinding().
				SetBuffer(meshData).SetOffset(offset).SetSlot(VertexAttributeSlot::TexCoord);
			offset += uvsByteSize;
		}
		VertexBufferBinding tangentStream{};
		if (tangents.size() > 0) {
			cmdList->WriteBuffer(meshData, tangents.data(), tangentsByteSize, offset);
			tangentStream = VertexBufferBinding().
				SetBuffer(meshData).SetOffset(offset).SetSlot(VertexAttributeSlot::Tangent);
			offset += tangentsByteSize;
		}

		assert(indices.size() > 0);
		cmdList->WriteBuffer(meshData, indices.data(), indexByteSize, offset);
		auto indexBufferViews = IndexBufferBinding().
			SetBuffer(meshData).SetOffset(offset).SetFormat(Format::R32_UINT);
		offset += indexByteSize;

		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);

		for(auto& mesh : meshes) {
			mesh->meshData = meshData;
			mesh->positionStream = positionStream;
			mesh->normalStream = normalStream;
			mesh->uvStream = uvStream;
			mesh->tangentStream = tangentStream;
			mesh->indexBufferViews = indexBufferViews;
		}

		return meshes;
	}

	void ProcessMaterial(
		Model& model,
		const std::string& filename,
		const aiScene* scene)
	{
		std::vector<std::vector<TextureHandle>> matTextures(scene->mNumMaterials, std::vector<TextureHandle>(ShaderResource::kNumTextures, nullptr));
		std::map<std::string, TextureHandle> uniqueTextures{};

		model.materials.resize(scene->mNumMaterials);
		for (UINT i = 0; i < scene->mNumMaterials; ++i) {
			auto& modelMaterial = model.materials[i];
			auto& material = scene->mMaterials[i];

			modelMaterial = std::make_shared<ShaderResource::MaterialData>();
			modelMaterial->baseColor = Math::Vector4{1, 1, 1, 1};
			modelMaterial->emissiveColor = Math::Vector4{0, 0, 0, 0};
			modelMaterial->normalTexScale = 1;
			modelMaterial->metallicFactor = 1;
			modelMaterial->roughnessFactor = 1;

			Math::Vector3 vector{};
			std::uint32_t num = 3;
			float value{};

			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_BASE_COLOR, (float*)&vector, &num)) {
				modelMaterial->baseColor = Math::Vector4{vector, 1};
			}
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_COLOR_EMISSIVE, (float*)&vector, &num)) {
				modelMaterial->emissiveColor = Math::Vector4{vector, 1};
			}
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_METALLIC_FACTOR, value)) {
				modelMaterial->metallicFactor = value;
			}
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_ROUGHNESS_FACTOR, value)) {
				modelMaterial->roughnessFactor = value;
			}
			aiString aiPath;
			std::filesystem::path texFilename;
			std::string texName;

			auto& srcHandle = matTextures[i];

			auto tryCreateTexture = [&](aiTextureType type) {
				ShaderResource::MaterialTex materialTex;
				switch (type) {
					case aiTextureType_DIFFUSE : 
					case aiTextureType_BASE_COLOR: materialTex = ShaderResource::kBaseColor; break;
					case aiTextureType_DIFFUSE_ROUGHNESS: materialTex = ShaderResource::kDiffuseRoughness; break;
					case aiTextureType_METALNESS: materialTex = ShaderResource::kMetalness; break;
					case aiTextureType_AMBIENT_OCCLUSION : materialTex = ShaderResource::kOcclusion; break;
					case aiTextureType_EMISSIVE: materialTex = ShaderResource::kEmissive; break;
					case aiTextureType_NORMALS: materialTex = ShaderResource::kNormal; break;
					default: materialTex = ShaderResource::kBaseColor; break;
				}
				if (material->GetTextureCount(type) == 0) {
					srcHandle[materialTex] = s_CommonTextures[materialTex];
					return false;
				}
				
				material->GetTexture(type, 0, &aiPath);

				TextureHandle texHandle = nullptr;
				// 纹理已经预先加载进来
				if (aiPath.data[0] == '*'){
					texName = filename;
					texName += aiPath.C_Str();
					char* pEndStr = nullptr;
					aiTexture* pTex = scene->mTextures[strtol(aiPath.data + 1, &pEndStr, 10)];
					TextureDesc texDesc{};
					texDesc.format = Format::RGBA8_UNORM;
					texDesc.width = pTex->mWidth;
					texDesc.height = pTex->mHeight;
					texHandle = TextureManager::LoadTextureFromMemory(texName, texDesc, pTex->pcData);

					srcHandle[materialTex] = texHandle;
				}
				else {	// 纹理通过文件名索引
					texFilename = filename;
					texFilename = texFilename.parent_path() / aiPath.C_Str();
					if(uniqueTextures.contains(filename)){
						texHandle = uniqueTextures[filename];
					}
					else{
						texHandle = TextureManager::LoadTextureFromFile(texFilename.string());
					}
					srcHandle[materialTex] = texHandle;
				}
				return true;
			};
			// 加载纹理
			if (!tryCreateTexture(aiTextureType_BASE_COLOR)) {
				tryCreateTexture(aiTextureType_DIFFUSE);
			}
			tryCreateTexture(aiTextureType_DIFFUSE_ROUGHNESS);
			tryCreateTexture(aiTextureType_METALNESS);
			tryCreateTexture(aiTextureType_AMBIENT_OCCLUSION);
			tryCreateTexture(aiTextureType_EMISSIVE);
			tryCreateTexture(aiTextureType_NORMALS);
		}

		for (auto& mesh : model.meshes) {
			int psoFlags = 0;
			std::uint32_t num = 1;
			mesh->textures = matTextures[mesh->materialIndex];
			auto& material = scene->mMaterials[mesh->materialIndex];
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_TWOSIDED, &psoFlags, &num)) {
				mesh->psoFlags |= (psoFlags == 0) ? mesh->psoFlags : kBothSide;
			}
			if(aiReturn_SUCCESS == material->Get(AI_MATKEY_OPACITY, &psoFlags, &num)) {
				mesh->psoFlags |= (psoFlags == 1) ? mesh->psoFlags : kAlphaBlend;
			}
		}
	}
	
}

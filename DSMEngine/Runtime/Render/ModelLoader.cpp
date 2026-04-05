#include "ModelLoader.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "Renderer/GraphicsRenderer.h"
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

	
    void ProcessNode(Model& model, aiNode* node, const aiScene* scene, ICommandList* cmdList, 
		const std::vector<std::array<TextureHandle, ShaderResource::kNumTextures>>& matTextures);
    std::vector<std::array<TextureHandle, ShaderResource::kNumTextures>> ProcessMaterial(
		Model& model,const std::string& filename,const aiScene* scene);
    std::vector<std::shared_ptr<Mesh>> CreateMesh(
		aiNode* node, 
		const aiScene* scene, 
		ICommandList* cmdList, 
		const std::vector<std::array<TextureHandle, ShaderResource::kNumTextures>>& matTextures);
    std::vector<std::shared_ptr<Mesh>> CreateMesh(const std::span<MeshData>& meshDatas, ICommandList* cmdList);

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

		auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("LoadModelFromGeometry"));
		cmdList->Open();
		model->meshes = CreateMesh({&meshData, 1}, cmdList);
		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);

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

			DSM_CORE_ASSERT(s_GraphicsDevice != nullptr);
			auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("LoadModel"));
			cmdList->Open();
			
			auto matTextures = ProcessMaterial(*model, filename, pScene);
			ProcessNode(*model, pScene->mRootNode, pScene, cmdList, matTextures);

			cmdList->Close();
			s_GraphicsDevice->ExecuteCommandList(cmdList);

			Math::AxisAlignedBox boundingBox{};
			for (const auto& mesh : model->meshes) {
				boundingBox = Math::AxisAlignedBox::Union(boundingBox, mesh->boundingBox);
			}
			model->boundingBox = boundingBox;
		}
		model->filePath = filename;

		return model;
	}

	void ProcessNode(
		Model& model, 
		aiNode* node, 
		const aiScene* scene, 
		ICommandList* cmdList, 
		const std::vector<std::array<TextureHandle, ShaderResource::kNumTextures>>& matTextures)
	{
		// 导入当前节点的网格
		auto meshes = CreateMesh(node, scene, cmdList, matTextures);
		if(!meshes.empty()) {
			model.meshes.append_range(meshes);
		}

		// 导入子节点的网格
		for (UINT i = 0; i < node->mNumChildren; ++i) {
			ProcessNode(model, node->mChildren[i], scene, cmdList, matTextures);
		}
	}

	void CreateMeshData(std::vector<std::shared_ptr<Mesh>>& meshes,
		const std::vector<Math::Vector3>& positions,
		const std::vector<Math::Vector3>& normals,
		const std::vector<Math::Vector2>& uvs,
		const std::vector<Math::Vector4>& tangents,
		const std::vector<uint32_t>& indices,
		ICommandList* cmdList)
	{
		if(meshes.empty() || cmdList == nullptr) {
			return;
		}

		std::uint32_t posByteSize = positions.size() * sizeof(Math::Vector3);
		std::uint32_t normalByteSize = normals.size() * sizeof(Math::Vector3);
		std::uint32_t uvsByteSize = uvs.size() * sizeof(Math::Vector2);
		std::uint32_t tangentsByteSize = tangents.size() * sizeof(Math::Vector4);
		std::uint32_t indexByteSize = indices.size() * sizeof(std::uint32_t);

		auto meshDataBufferSize = posByteSize + normalByteSize + uvsByteSize + tangentsByteSize + indexByteSize;
		auto meshData = s_GraphicsDevice->CreateBuffer(
			BufferDesc().SetByteSize(meshDataBufferSize).SetDebugName("MeshData" + meshes[0]->name));
		
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

		for(auto& mesh : meshes) {
			mesh->meshData = meshData;
			mesh->positionStream = positionStream;
			mesh->normalStream = normalStream;
			mesh->uvStream = uvStream;
			mesh->tangentStream = tangentStream;
			mesh->indexBufferViews = indexBufferViews;
		}
	}

	std::vector<std::shared_ptr<Mesh>> CreateMesh(const std::span<MeshData>& meshDatas, ICommandList* cmdList)
	{
		if (meshDatas.empty() || cmdList == nullptr) 
			return {};
		
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

		CreateMeshData(meshes, positions, normals, uvs, tangents, indices, cmdList);

		return meshes;
	}

	std::vector<std::shared_ptr<Mesh>> CreateMesh(
		aiNode* node, 
		const aiScene* scene, 
		ICommandList* cmdList,
		const std::vector<std::array<TextureHandle, ShaderResource::kNumTextures>>& matTextures)
	{
		if (node == nullptr || node->mNumMeshes <= 0 || scene == nullptr || cmdList == nullptr) 
			return {};
		
		std::vector<Math::Vector3> positions{};
		std::vector<Math::Vector3> normals{};
		std::vector<Math::Vector2> uvs{};
		std::vector<Math::Vector4> tangents{};
		std::vector<uint32_t> indices{};
		auto firstMesh = scene->mMeshes[node->mMeshes[0]];
		positions.reserve(node->mNumMeshes * firstMesh->mNumVertices);
		normals.reserve(node->mNumMeshes * firstMesh->mNumVertices);
		uvs.reserve(node->mNumMeshes * firstMesh->mNumVertices);
		indices.reserve(node->mNumMeshes * firstMesh->mNumFaces * 3);

		UINT preIndexCount = 0;
		UINT preVertexCount = 0;
		std::vector<std::shared_ptr<Mesh>> meshes;
		meshes.reserve(node->mNumMeshes);
		for(size_t i = 0; i < node->mNumMeshes; ++i) {
			auto aiMesh = scene->mMeshes[node->mMeshes[i]];
			// 设置网格使用的顶点数据类型
			uint16_t psoFlags = 0;
			auto reserveAndSetFlag = [&aiMesh, &psoFlags](bool hasData, PSOFlags flag, auto& data) {
				if (hasData) {
					psoFlags |= flag;
					data.reserve(data.size() + aiMesh->mNumVertices);
				}
			};
			reserveAndSetFlag(aiMesh->HasPositions(), kHasPosition, positions);
			DSM_CORE_ASSERT((psoFlags & kHasPosition) != 0);
			reserveAndSetFlag(aiMesh->HasNormals(), kHasNormal, normals);
			reserveAndSetFlag(aiMesh->HasTextureCoords(0), kHasUV, uvs);
			reserveAndSetFlag(aiMesh->HasTangentsAndBitangents(), kHasTangent, tangents);
			int flag = 0;
			uint32_t num = 1;
			auto& material = scene->mMaterials[aiMesh->mMaterialIndex];
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_TWOSIDED, &flag, &num)) {
				psoFlags |= (flag == 0) ? psoFlags : kBothSide;
			}
			if(aiReturn_SUCCESS == material->Get(AI_MATKEY_OPACITY, &flag, &num)) {
				psoFlags |= (flag == 1) ? psoFlags : kAlphaBlend;
			}

			// 创建网格
			std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
			mesh->name = aiMesh->mName.C_Str();
			mesh->materialIndex = aiMesh->mMaterialIndex;
			mesh->indexOffset = preIndexCount;
			mesh->vertexOffset = preVertexCount;
			mesh->psoFlags = psoFlags;
			auto numIndex = aiMesh->mFaces->mNumIndices;
			mesh->indexCount = aiMesh->mNumFaces * numIndex;
			const auto& aabb = aiMesh->mAABB;
			mesh->boundingBox = Math::AxisAlignedBox{
				Math::Vector3{aabb.mMin.x, aabb.mMin.y, aabb.mMin.z},
				Math::Vector3{aabb.mMax.x, aabb.mMax.y, aabb.mMax.z}};
			const auto& textures = matTextures[mesh->materialIndex];
			mesh->textures = std::vector<TextureHandle>{textures.begin(), textures.end()};

			preIndexCount += mesh->indexCount;
			preVertexCount += aiMesh->mNumVertices;
			
			// 获取顶点数据和索引数据
			for(size_t i = 0; i < aiMesh->mNumVertices; ++i){
				positions.emplace_back(Math::Vector3{aiMesh->mVertices[i].x, aiMesh->mVertices[i].y, aiMesh->mVertices[i].z});
				if (HasFlags(PSOFlags{mesh->psoFlags}, kHasNormal)) {
					normals.emplace_back(Math::Vector3{aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z});
				}
				if (HasFlags(PSOFlags{mesh->psoFlags}, kHasUV)) {
					uvs.emplace_back(Math::Vector2{aiMesh->mTextureCoords[0][i].x, aiMesh->mTextureCoords[0][i].y});
				}
				if (HasFlags(PSOFlags{mesh->psoFlags}, kHasTangent)) {
					tangents.emplace_back(Math::Vector4{aiMesh->mTangents[i].x, aiMesh->mTangents[i].y, aiMesh->mTangents[i].z, 1.0f});
				}
			}
			auto preIndexSize = indices.size();
			indices.resize(preIndexSize + mesh->indexCount);
			for (size_t i = 0; i < aiMesh->mNumFaces; ++i) {
				memcpy(indices.data() + preIndexSize + i * numIndex, aiMesh->mFaces[i].mIndices, sizeof(uint32_t) * numIndex);
			}

			meshes.push_back(mesh);
		}

		CreateMeshData(meshes, positions, normals, uvs, tangents, indices, cmdList);

		return meshes;
	}

	std::vector<std::array<TextureHandle, ShaderResource::kNumTextures>> ProcessMaterial(Model& model, const std::string& filename, const aiScene* scene)
	{
		std::vector<std::array<TextureHandle, ShaderResource::kNumTextures>> matTextures(
			scene->mNumMaterials, s_CommonTextures);
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

		return matTextures;
	}
	
}

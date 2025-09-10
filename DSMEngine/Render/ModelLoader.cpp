#include "ModelLoader.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "Material.h"
#include "Renderer.h"
#include "Geometry.h"
#include "Math/MathCommon.h"
#include "Graphics/CommandList.h"
#include "Graphics/Device.h"
#include "TextureManager.h"
#include <filesystem>



namespace DSM::ModelLoader {
	
	struct MeshData
	{
		std::string name{};
		std::vector<Math::Vector3> positions{};
		std::vector<Math::Vector3> normals{};
		std::vector<Math::Vector2> texcoords{};
		std::vector<Math::Vector4> tangents{};
		std::vector<Math::Vector3> bitangents{};
		std::vector<uint32_t> indices{};
		uint32_t materialIndex = 0;
		uint16_t psoFlags = 0;
	};


	static DeviceHandle s_GraphicsDevice;
    void Init(IDevice *device)
    {
        s_GraphicsDevice = device;
    }


    void Destroy()
	{
		s_GraphicsDevice = nullptr;
	}

	
    void ProcessNode(Model& model, aiNode* node, const aiScene* scene);
    void ProcessMaterial(Model& model,const std::string& filename,const aiScene* scene);
    MeshData ProcessMesh(aiMesh* mesh);
    void CreateMesh(Mesh& mesh, const std::span<MeshData>& meshDatas);

	std::shared_ptr<Model> LoadModelFromGeometry(const std::string& name, const Geometry::GeometryMesh& geometryMesh)
	{
		if (geometryMesh.vertices.empty()){
			return nullptr;
		}

		auto model = std::make_shared<Model>();
		model->name = name;
		model->materials.emplace_back(std::make_shared<Material>());
		auto& mesh = model->meshes.emplace_back(std::make_shared<Mesh>());
		mesh->name = name;

		MeshData meshData{};
		meshData.indices = geometryMesh.indices32;
		meshData.name = name;
		meshData.materialIndex = 0;
		meshData.psoFlags |= kHasPosition | kHasNormal | kHasTangent | kHasUV;
		for (const auto& vertex : geometryMesh.vertices) {
			meshData.positions.push_back(vertex.position);
			meshData.normals.push_back(vertex.normal);
			meshData.texcoords.push_back(vertex.texCoord);
			meshData.tangents.push_back(vertex.tangent);
			meshData.bitangents.push_back(vertex.biTangent);
		}

		CreateMesh(*mesh, {&meshData, 1});
		
		return model;
	}

    std::shared_ptr<Model> LoadModel(const std::string &filename)
    {
		auto model = std::make_shared<Model>();
		
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
			OutputDebugStringA(warning.c_str());
			return nullptr;
		}

		model->name = pScene->mRootNode->mName.C_Str();

		ProcessNode(*model, pScene->mRootNode, pScene);
		ProcessMaterial(*model, filename, pScene);

		return model;
	}

	void ProcessNode(Model& model, aiNode* node, const aiScene* scene)
	{
		// 导入当前节点的网格
		auto mesh = std::make_shared<Mesh>();
		mesh->name = node->mName.C_Str();
		std::vector<MeshData> meshDatas{};
		meshDatas.reserve(node->mNumMeshes);
		for (UINT i = 0; i < node->mNumMeshes; ++i) {
			meshDatas.emplace_back(ProcessMesh(scene->mMeshes[node->mMeshes[i]]));
		}

		if (!meshDatas.empty()) {
			CreateMesh(*mesh, meshDatas);
			model.meshes.push_back(std::move(mesh));
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

		meshData.materialIndex = mesh->mMaterialIndex;

		return meshData;
	}

	void CreateMesh(Mesh& mesh, const std::span<MeshData>& meshDatas)
	{
		if (meshDatas.empty()) return;
		
		std::vector<Math::Vector3> positions{};
		std::vector<Math::Vector3> normals{};
		std::vector<Math::Vector2> uvs{};
		std::vector<Math::Vector4> tangents{};
		std::vector<uint32_t> indices{};

		UINT preIndexCount = 0;
		UINT preVertexCount = 0;
		for (const auto& meshData : meshDatas) {
			auto indexCount = meshData.indices.size();
			Mesh::SubMesh submesh;
			submesh.materialIndex = meshData.materialIndex;
			submesh.indexCount = indexCount;
			submesh.indexOffset = preIndexCount;
			submesh.vertexOffset = preVertexCount;
			mesh.subMeshes.insert(std::make_pair(meshData.name, std::move(submesh)));

			preIndexCount += indexCount;
			preVertexCount += meshData.positions.size();

			std::uint16_t posNormalFlags = kHasPosition;
			DSM_CORE_ASSERT((meshData.psoFlags & posNormalFlags) != 0);
			mesh.psoFlags = 0xffff;
			mesh.psoFlags &= meshData.psoFlags;
			positions.insert(positions.end(), meshData.positions.begin(), meshData.positions.end());
			normals.insert(normals.end(), meshData.normals.begin(), meshData.normals.end());
			uvs.insert(uvs.begin(), meshData.texcoords.begin(), meshData.texcoords.end());
			tangents.insert(tangents.end(), meshData.tangents.begin(), meshData.tangents.end());
			indices.insert(indices.end(), meshData.indices.begin(), meshData.indices.end());
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
		mesh.meshData = s_GraphicsDevice->CreateBuffer(
			BufferDesc().SetByteSize(meshDataBufferSize).SetDebugName("MeshData" + mesh.name));
		
		std::uint32_t offset = 0;
		cmdList->WriteBuffer(mesh.meshData, positions.data(), posByteSize, offset);
		mesh.positionStream = VertexBufferBinding().
			SetBuffer(mesh.meshData).SetOffset(offset).SetSlot(VertexAttributeSlot::Position);
		offset += posByteSize;

		if (normals.size() > 0) {
			cmdList->WriteBuffer(mesh.meshData, normals.data(), normalByteSize, offset);
			mesh.normalStream = VertexBufferBinding().
				SetBuffer(mesh.meshData).SetOffset(offset).SetSlot(VertexAttributeSlot::Normal);
			offset += normalByteSize;
		}
		if (uvs.size() > 0) {
			cmdList->WriteBuffer(mesh.meshData, uvs.data(), uvsByteSize, offset);
			mesh.uvStream = VertexBufferBinding().
				SetBuffer(mesh.meshData).SetOffset(offset).SetSlot(VertexAttributeSlot::TexCoord);
			offset += uvsByteSize;
		}
		if (tangents.size() > 0) {
			cmdList->WriteBuffer(mesh.meshData, tangents.data(), tangentsByteSize, offset);
			mesh.tangentStream = VertexBufferBinding().
				SetBuffer(mesh.meshData).SetOffset(offset).SetSlot(VertexAttributeSlot::Tangent);
			offset += tangentsByteSize;
		}

		cmdList->WriteBuffer(mesh.meshData, indices.data(), indexByteSize, offset);
		mesh.indexBufferViews = IndexBufferBinding().
			SetBuffer(mesh.meshData).SetOffset(offset).SetFormat(Format::R32_UINT);
		offset += indexByteSize;

		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);
	}

	void ProcessMaterial(
		Model& model,
		const std::string& filename,
		const aiScene* scene)
	{
		std::vector<std::vector<TextureHandle>> matTextures(scene->mNumMaterials, std::vector<TextureHandle>(kNumTextures));
		
		model.materials.resize(scene->mNumMaterials);
		for (UINT i = 0; i < scene->mNumMaterials; ++i) {
			auto& modelMaterial = model.materials[i];
			auto& material = scene->mMaterials[i];

			modelMaterial = std::make_shared<Material>();
			
			Math::Vector3 vector{};
			std::uint32_t num = 3;
			float value{};

			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_BASE_COLOR, (float*)&vector, &num)) {
				modelMaterial->baseColor = Math::Vector4{vector, 1};
			}
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_COLOR_EMISSIVE, (float*)&vector, &num)) {
				modelMaterial->emissiveColor = vector;
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

			TextureHandle defaultTexture[kNumTextures] = {
				TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
				TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
				TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
				TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
				TextureManager::GetDefaultTexture(TextureManager::kBlackTransparent2D),
				TextureManager::GetDefaultTexture(TextureManager::kDefaultNormalTex)
			};

			auto& srcHandle = matTextures[i];

			auto tryCreateTexture = [&](aiTextureType type) {
				MaterialTex materialTex;
				switch (type) {
					case aiTextureType_BASE_COLOR: materialTex = kBaseColor; break;
					case aiTextureType_DIFFUSE_ROUGHNESS: materialTex = kDiffuseRoughness; break;
					case aiTextureType_METALNESS: materialTex = kMetalness; break;
					case aiTextureType_AMBIENT_OCCLUSION : materialTex = kOcclusion; break;
					case aiTextureType_EMISSIVE: materialTex = kEmissive; break;
					case aiTextureType_NORMALS: materialTex = kNormal; break;
					default: materialTex = kBaseColor; break;
				}
				if (material->GetTextureCount(type) == 0) {
					srcHandle[materialTex] = defaultTexture[materialTex];
					return;
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
					texHandle = TextureManager::LoadTextureFromFile(texFilename.string());

					srcHandle[materialTex] = texHandle;
				}
			};
			// 加载纹理
			tryCreateTexture(aiTextureType_BASE_COLOR);
			tryCreateTexture(aiTextureType_DIFFUSE_ROUGHNESS);
			tryCreateTexture(aiTextureType_METALNESS);
			tryCreateTexture(aiTextureType_AMBIENT_OCCLUSION);
			tryCreateTexture(aiTextureType_EMISSIVE);
			tryCreateTexture(aiTextureType_NORMALS);
		}

		for (auto& mesh : model.meshes) {
			int psoFlags = 0;
			std::uint32_t num = 1;
			for (auto& [name, submesh] : mesh->subMeshes) {
				submesh.textures = matTextures[submesh.materialIndex];
				auto& material = scene->mMaterials[submesh.materialIndex];
				if (aiReturn_SUCCESS == material->Get(AI_MATKEY_TWOSIDED, &psoFlags, &num)) {
					mesh->psoFlags |= (psoFlags == 0) ? mesh->psoFlags : kBothSide;
				}
				if(aiReturn_SUCCESS != material->Get(AI_MATKEY_OPACITY, &psoFlags, &num)) {
					mesh->psoFlags |= (psoFlags == 0) ? mesh->psoFlags : kAlphaBlend;
				}
			}
		}

		std::vector<Material> materialConstants(model.materials.size());
		for (std::size_t i = 0; i < model.materials.size(); i++) {
			memcpy(&materialConstants[i], model.materials[i].get(), sizeof(Material));
		}
		auto matDataSize = sizeof(Material) * materialConstants.size();
		model.materialData = s_GraphicsDevice->CreateBuffer(BufferDesc().
			SetByteSize(matDataSize).
			SetIsConstantBuffer(true).
			SetDebugName("Model MaterialData" + model.name));

		auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("Init MaterialConstants"));
		cmdList->Open();
		cmdList->WriteBuffer(model.materialData, materialConstants.data(), matDataSize);
		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);
	}
	
}

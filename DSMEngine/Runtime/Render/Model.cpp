#include "Model.h"
#include "Mesh.h"
#include "Geometry.h"
#include "Runtime/Core/Macro.h"
#include "TextureManager.h"
#include "Runtime/Core/InstrumentorTimer.h"

#include <cstring>
#include <filesystem>
#include <map>
#include <thread>
#include <future>
#include <stack>
#include <execution>
#include <ranges>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace DSM {
	void Model::ProcessMaterial(Model& model, const std::string& filename, const aiScene* scene)
	{
		if(scene->mNumMaterials == 0){
			return;
		}

		auto processMaterialFunc = [&filename, &scene](const aiMaterial* material) {
			auto modelMat = std::make_shared<Material>(Shader::Find("Shaders/ForwardShader/Passes/LitPass.hlsl"));

			Math::Vector3 vector{};
			uint32_t num = 3;
			float value{};
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_BASE_COLOR, (float*)&vector, &num)) {
				modelMat->SetBaseColor(Math::Vector4{vector, 1});
			}
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_COLOR_EMISSIVE, (float*)&vector, &num)) {
				modelMat->SetEmissiveColor(Math::Vector4{vector, 1});
			}
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_METALLIC_FACTOR, value)) {
				modelMat->SetMetallicFactor(value);
			}
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_ROUGHNESS_FACTOR, value)) {
				modelMat->SetRoughnessFactor(value);	
			}
			if (aiReturn_SUCCESS == material->Get(AI_MATKEY_TWOSIDED, value)) {
				modelMat->SetBothSide(value != 0);
			}
			if(aiReturn_SUCCESS == material->Get(AI_MATKEY_OPACITY, value)) {
				modelMat->SetTransparent(value < 1.0f);
			}

			std::array<TextureHandle, ShaderResource::kNumTextures> textures = sm_CommonTextures;
			auto tryCreateTexture = [&](aiTextureType type) {
				ShaderResource::MaterialTex materialTex;
				switch (type) {
				case aiTextureType_DIFFUSE:
				case aiTextureType_BASE_COLOR: materialTex = ShaderResource::kBaseColor; break;
				case aiTextureType_DIFFUSE_ROUGHNESS: materialTex = ShaderResource::kDiffuseRoughness; break;
				case aiTextureType_METALNESS: materialTex = ShaderResource::kMetalness; break;
				case aiTextureType_AMBIENT_OCCLUSION: materialTex = ShaderResource::kOcclusion; break;
				case aiTextureType_EMISSIVE: materialTex = ShaderResource::kEmissive; break;
				case aiTextureType_NORMALS: materialTex = ShaderResource::kNormal; break;
				default: materialTex = ShaderResource::kBaseColor; break;
				}

				if (material->GetTextureCount(type) == 0) {
					textures[materialTex] = sm_CommonTextures[materialTex];
					return false;
				}

				aiString aiPath{};
				material->GetTexture(type, 0, &aiPath);
				TextureHandle texHandle = nullptr;
				if (aiPath.data[0] == '*') {
					auto texName = filename;
					texName += aiPath.C_Str();
					char* pEndStr = nullptr;
					aiTexture* pTex = scene->mTextures[strtol(aiPath.data + 1, &pEndStr, 10)];
					TextureDesc texDesc{};
					texDesc.format = Format::RGBA8_UNORM;
					texDesc.width = pTex->mWidth;
					texDesc.height = pTex->mHeight;
					texHandle = TextureManager::LoadTextureFromMemory(texName, texDesc, pTex->pcData);
				}
				else {
					auto texFilename = std::filesystem::path(filename).parent_path() / aiPath.C_Str();
					const auto texKey = texFilename.string();
					texHandle = TextureManager::LoadTextureFromFile(texKey);
				}

				textures[materialTex] = texHandle;
				return true;
			};

			if (!tryCreateTexture(aiTextureType_BASE_COLOR)) {
				tryCreateTexture(aiTextureType_DIFFUSE);
			}
			tryCreateTexture(aiTextureType_DIFFUSE_ROUGHNESS);
			tryCreateTexture(aiTextureType_METALNESS);
			tryCreateTexture(aiTextureType_AMBIENT_OCCLUSION);
			tryCreateTexture(aiTextureType_EMISSIVE);
			tryCreateTexture(aiTextureType_NORMALS);
			modelMat->SetTextures(std::move(textures));

			return modelMat;
		};
		
		// 并行处理所有材质，避免处理复杂材质时阻塞主线程过久
		const auto maxThreadCount = std::max(std::thread::hardware_concurrency(), 1u) - 1;
		std::vector<std::future<std::shared_ptr<Material>>> materialFutures{};
		auto begin = scene->mNumMaterials - std::min(maxThreadCount, scene->mNumMaterials);
		for (size_t i = begin; i < scene->mNumMaterials; ++i) {
			materialFutures.emplace_back(std::async(std::launch::async, processMaterialFunc, scene->mMaterials[i]));
		}

		// 需要保持顺序，因为mesh中material索引是有序的
		model.materials.resize(scene->mNumMaterials);
		for(size_t i = 0; i < begin; ++i) {
			model.materials[i] = processMaterialFunc(scene->mMaterials[i]);
		}
		for(size_t i = begin; i < scene->mNumMaterials; ++i) {
			model.materials[i] = materialFutures[i - begin].get();
		}
	}

    void Model::ProcessNode(Model &model, const aiScene *scene)
    {
		using MeshResult = std::pair<std::shared_ptr<Mesh>, std::vector<uint32_t>>;

		auto consumeMeshFunc = [&model](MeshResult&& meshResult) {
			auto [mesh, materialIndices] = std::move(meshResult);
			if(mesh != nullptr) {
				model.meshMaterialIndices.emplace_back(std::move(materialIndices));
				Math::AxisAlignedBox::Union(model.boundingBox, mesh->bounds);
				model.meshes.emplace_back(std::move(mesh));
			}
		};

		const size_t maxThreadCount = std::max(std::thread::hardware_concurrency(), 1u) - 1;
		std::list<std::future<MeshResult>> meshFutures{};

		std::stack<const aiNode*> nodeStack{};
		nodeStack.push(scene->mRootNode);
		while (!nodeStack.empty()) {
			const auto* node = nodeStack.top();
			nodeStack.pop();
			if (node == nullptr) {
				continue;
			}

			if(node->mNumMeshes > 0) {
				// 回收已经完成的任务
				if(meshFutures.size() >= maxThreadCount) {
					bool collected = false;
					for(auto it = meshFutures.begin(); it != meshFutures.end();) {
						if(it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
							consumeMeshFunc(it->get());
							it = meshFutures.erase(it);
							collected = true;
						}
						else {
							++it;
						}
					}
					// 如果没有任务完成，则等待最早的一个任务完成
					if(!collected){
						consumeMeshFunc(meshFutures.front().get());
						meshFutures.pop_front();
					}
				}
				meshFutures.emplace_back(std::async(GetMeshFromNode, node, scene));
			}

			nodeStack.push_range(std::span(node->mChildren, node->mNumChildren));
		}

		// 等待剩余的任务完成
		for (auto& meshFuture : meshFutures) {
			consumeMeshFunc(meshFuture.get());
		}
    }

    std::pair<std::shared_ptr<Mesh>, std::vector<uint32_t>> Model::GetMeshFromNode(const aiNode *node, const aiScene *scene)
    {
		using MeshResult = std::pair<std::shared_ptr<Mesh>, std::vector<uint32_t>>;
		if (node == nullptr || scene == nullptr || node->mNumMeshes <= 0) {
			return MeshResult{nullptr, std::vector<uint32_t>{}};
		}

		std::vector<Math::Vector3> positions{};
		std::vector<Math::Vector3> normals{};
		std::vector<Math::Vector2> uvs{};
		std::vector<Math::Vector4> tangents{};
		std::vector<uint32_t> indices{};
		auto mesh = std::make_shared<Mesh>();
		std::vector<uint32_t> materialIndices{};
		mesh->SetName(std::string(node->mName.C_Str()));
		mesh->SetIndexFormat(Format::R32_UINT);
		for (size_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
			auto aiMesh = scene->mMeshes[node->mMeshes[meshIndex]];
			if (aiMesh == nullptr || !aiMesh->HasPositions()) {
				continue;
			}
			indices.clear();

			const auto baseVertex = positions.size();
			const auto baseNormal = normals.size();
			const auto baseUV = uvs.size();
			const auto baseTangent = tangents.size();
			positions.resize(positions.size() + aiMesh->mNumVertices);
			if (aiMesh->HasNormals()) {
				normals.resize(normals.size() + aiMesh->mNumVertices);
			}
			if (aiMesh->HasTextureCoords(0)) {
				uvs.resize(uvs.size() + aiMesh->mNumVertices);
			}
			if (aiMesh->HasTangentsAndBitangents()) {
				tangents.resize(tangents.size() + aiMesh->mNumVertices);
			}

			auto processVertexFunc = [&](size_t i) {
				positions[baseVertex + i] = Math::Vector3{ aiMesh->mVertices[i].x, aiMesh->mVertices[i].y, aiMesh->mVertices[i].z };
				if (aiMesh->HasNormals()) {
					normals[baseNormal + i] = Math::Vector3{ aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z };
				}
				if (aiMesh->HasTextureCoords(0)) {
					uvs[baseUV + i] = Math::Vector2{ aiMesh->mTextureCoords[0][i].x, aiMesh->mTextureCoords[0][i].y };
				}
				if (aiMesh->HasTangentsAndBitangents()) {
					tangents[baseTangent + i] = Math::Vector4{ aiMesh->mTangents[i].x, aiMesh->mTangents[i].y, aiMesh->mTangents[i].z, 1.0f };
				}
			};
			auto indexView = std::views::iota(size_t{0}, static_cast<size_t>(aiMesh->mNumVertices));
			constexpr size_t parallelThreshold = 10000;
			if (aiMesh->mNumVertices >= parallelThreshold) {
				std::for_each(std::execution::par, indexView.begin(), indexView.end(), processVertexFunc);
			}
			else {	
				std::for_each(indexView.begin(), indexView.end(), processVertexFunc);
			}

			const auto& aabb = aiMesh->mAABB;
			auto bounds = Math::AxisAlignedBox{
				Math::Vector3{aabb.mMin.x, aabb.mMin.y, aabb.mMin.z},
				Math::Vector3{aabb.mMax.x, aabb.mMax.y, aabb.mMax.z}
			};

			for (size_t i = 0; i < aiMesh->mNumFaces; ++i) {
				const auto& face = aiMesh->mFaces[i];
				for (size_t j = 0; j < face.mNumIndices; ++j) {
					indices.emplace_back(face.mIndices[j]);
				}
			}

			mesh->SetIndices<uint32_t>(indices, PrimitiveType::TriangleList, meshIndex, bounds, positions.size() - aiMesh->mNumVertices);

			materialIndices.emplace_back(aiMesh->mMaterialIndex);
		}

		mesh->SetVertices(std::move(positions))
			.SetNormals(std::move(normals))
			.SetUVs(std::move(uvs))
			.SetTangents(std::move(tangents));
		mesh->UploadBuffer();

		return MeshResult{mesh, materialIndices};
    }

    std::shared_ptr<Model> Model::LoadModelFromGeometry(
		const std::string& name,
		Geometry::GeometryMesh geometryMesh,
		std::shared_ptr<Material> material)
	{
		if (geometryMesh.vertices.empty()) {
			return nullptr;
		}

		auto model = std::make_shared<Model>();
		model->name = name;

		if (material == nullptr) {
			material = std::make_shared<Material>(Shader::Find("Shaders/ForwardShader/Passes/LitPass.hlsl"));
		}
		model->materials.emplace_back(material);

		auto mesh = std::make_shared<Mesh>();
		mesh->SetName(name)
			.SetIndexFormat(Format::R32_UINT)
			.SetIndices<uint32_t>(geometryMesh.indices32, PrimitiveType::TriangleList, 0);

		for(const auto& vertex : geometryMesh.vertices){
			mesh->vertices.emplace_back(vertex.position);
			mesh->normals.emplace_back(vertex.normal);
			mesh->uv.emplace_back(vertex.texCoord);
			mesh->tangents.emplace_back(vertex.tangent);
		}

		mesh->UploadBuffer();
		model->meshes.emplace_back(mesh);
		model->meshMaterialIndices.emplace_back(std::vector<uint32_t>{0});
		model->boundingBox = mesh->bounds;

		return model;
	}

	std::shared_ptr<Model> Model::LoadModel(const std::string& filename)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(
			filename,
			aiProcess_ConvertToLeftHanded |
			aiProcess_GenBoundingBoxes |
			aiProcess_Triangulate |
			aiProcess_ImproveCacheLocality |
			aiProcess_SortByPType);

		if (scene == nullptr || !scene->HasMeshes()) {
			std::string warning = "[Warning]: Failed to load \"";
			warning += filename;
			warning += "\"\n";
			DSM_CORE_WARN(warning);
			return nullptr;
		}

		auto model = std::make_shared<Model>();
		model->name = scene->mRootNode != nullptr ? scene->mRootNode->mName.C_Str() : "";
		model->filePath = filename;

		ProcessMaterial(*model, filename, scene);
		ProcessNode(*model, scene);

		return model;
	}

	void Model::Create(IDevice* device)
	{
		sm_Device = device;
		Mesh::Create(device);
		sm_CommonTextures = {
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kBlackTransparent2D),
			TextureManager::GetDefaultTexture(TextureManager::kDefaultNormalTex)
		};
	}

	void Model::Destroy()
	{
		Mesh::Destroy();
		sm_Device = nullptr;
		sm_CommonTextures.fill(nullptr);
	}
}

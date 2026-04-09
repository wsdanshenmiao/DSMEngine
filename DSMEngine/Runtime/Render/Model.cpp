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

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace DSM {
	void Model::ProcessMaterial(Model& model, const std::string& filename, const aiScene* scene)
	{
		std::map<std::string, TextureHandle> uniqueTextures{};

		model.materials.reserve(scene->mNumMaterials);
		for (size_t i = 0; i < scene->mNumMaterials; ++i) {
			auto& material = scene->mMaterials[i];
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
					if (uniqueTextures.contains(texKey)) {
						texHandle = uniqueTextures[texKey];
					}
					else {
						texHandle = TextureManager::LoadTextureFromFile(texKey);
						uniqueTextures[texKey] = texHandle;
					}
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

			model.materials.push_back(modelMat);
		}
	}

	auto Model::ProcessNode(const Model& model, const aiNode* node, const aiScene* scene)
	{
		using MeshMaterialPair = std::pair<std::vector<std::shared_ptr<Mesh>>, std::vector<std::vector<uint32_t>>>;
		MeshMaterialPair result{};
		if (node == nullptr || scene == nullptr) {
			return result;
		}

		std::vector<std::future<MeshMaterialPair>> childFutures(node->mNumChildren);
		for (size_t i = 0; i < node->mNumChildren; ++i) {
			childFutures[i] = std::async(std::launch::async, [&, i]() {
				return ProcessNode(model, node->mChildren[i], scene);
			});
		}

		if (node->mNumMeshes > 0) {
			std::vector<Math::Vector3> positions{};
			std::vector<Math::Vector3> normals{};
			std::vector<Math::Vector2> uvs{};
			std::vector<Math::Vector4> tangents{};
			std::vector<uint32_t> indices{};
			auto mesh = std::make_shared<Mesh>();
			mesh->SetName(std::string(node->mName.C_Str()));
			mesh->SetIndexFormat(Format::R32_UINT);
			std::vector<uint32_t> materialIndices{};
			for (size_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
				auto aiMesh = scene->mMeshes[node->mMeshes[meshIndex]];
				if (aiMesh == nullptr || !aiMesh->HasPositions()) {
					continue;
				}
				indices.clear();

				const bool hasNormal = aiMesh->HasNormals();
				const bool hasUV = aiMesh->HasTextureCoords(0);
				const bool hasTangent = aiMesh->HasTangentsAndBitangents();
				positions.reserve(positions.size() + aiMesh->mNumVertices);
				if (hasNormal) {
					normals.reserve(normals.size() + aiMesh->mNumVertices);
				}
				if (hasUV) {
					uvs.reserve(uvs.size() + aiMesh->mNumVertices);
				}
				if (hasTangent) {
					tangents.reserve(tangents.size() + aiMesh->mNumVertices);
				}

				for (size_t i = 0; i < aiMesh->mNumVertices; ++i) {
					positions.emplace_back(Math::Vector3{ aiMesh->mVertices[i].x, aiMesh->mVertices[i].y, aiMesh->mVertices[i].z });
					if (hasNormal) {
						normals.emplace_back(Math::Vector3{ aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z });
					}
					if (hasUV) {
						uvs.emplace_back(Math::Vector2{ aiMesh->mTextureCoords[0][i].x, aiMesh->mTextureCoords[0][i].y });
					}
					if (hasTangent) {
						tangents.emplace_back(Math::Vector4{ aiMesh->mTangents[i].x, aiMesh->mTangents[i].y, aiMesh->mTangents[i].z, 1.0f });
					}
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

			mesh->SetVertices(std::move(positions));
			mesh->SetNormals(std::move(normals));
			mesh->SetUVs(std::move(uvs));
			mesh->SetTangents(std::move(tangents));
			mesh->UploadBuffer();

			result.first.push_back(mesh);
			result.second.push_back(materialIndices);
		}

		for(auto& future : childFutures) {
			auto childResult = future.get();
			result.first.append_range(std::move(childResult.first));
			result.second.append_range(std::move(childResult.second));
		}

		return result;
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
		mesh->SetName(name);
		mesh->SetIndexFormat(Format::R32_UINT);
		mesh->SetIndices<uint32_t>(geometryMesh.indices32, PrimitiveType::TriangleList, 0);

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

		InstrumentationTimer timer("Material Loading");
		ProcessMaterial(*model, filename, scene);
		timer.Stop();

		InstrumentationTimer timer2("Mesh Loading");
		auto result = ProcessNode(*model, scene->mRootNode, scene);

		model->meshes = std::move(result.first);
		model->meshMaterialIndices = std::move(result.second);
		for(const auto& mesh : model->meshes) {
			Math::AxisAlignedBox::Union(model->boundingBox, mesh->bounds);
		}
		timer2.Stop();

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

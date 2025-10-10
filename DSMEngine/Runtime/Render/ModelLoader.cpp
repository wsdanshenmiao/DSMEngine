#include "ModelLoader.h"
#include "assimp/postprocess.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "Material.h"
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
	static const std::string s_ModelCacheDir = std::filesystem::current_path().string() + "\\Assets\\Models\\";
	static std::array<TextureHandle, kNumTextures> s_CommonTextures;

	struct ModelFileHeader
	{
		Math::AxisAlignedBox boundingBox{};
		uint64_t dataSize{};
		size_t meshCount{};
		size_t materialCount{};
		uint32_t modelNameSize{};
	};

	struct MeshFileHeader
	{
		Math::AxisAlignedBox boundingBox{};
		uint64_t dataSize{};
		uint64_t positionOffset{};
		uint64_t normalOffset{};
		uint64_t texcoordOffset{};
		uint64_t tangentOffset{};
		uint64_t indexOffset{};
		uint32_t nameSize{};
		uint32_t positionSlot{};
		uint32_t normalSlot{};
		uint32_t texcoordSlot{};
		uint32_t tangentSlot{};
		uint32_t submeshCount{};
		uint16_t psoFlags{};
		Format indexFormat{};
	};

	struct SubmeshFileHeader
	{
		uint32_t nameSize{};
		uint32_t indexCount;
		uint32_t indexOffset;
		uint32_t vertexOffset;
		uint16_t materialIndex;
		std::array<uint32_t, kNumTextures> texFilenameSizes{};
	};

	bool SaveModelToFile(const Model& model, const std::string& filename)
	{
		std::filesystem::path modelPath = s_ModelCacheDir + filename;
		if (!std::filesystem::exists(modelPath.parent_path())) {
			std::filesystem::create_directories(modelPath.parent_path());
		}
		std::ofstream ofs(modelPath, std::ios::binary);
		if (!ofs.is_open()) {
			return false;
		}

		// Write model data
		ModelFileHeader modelHeader{};
		modelHeader.boundingBox = model.boundingBox;
		modelHeader.modelNameSize = model.name.size();
		modelHeader.meshCount = model.meshes.size();
		modelHeader.materialCount = model.materials.size();
		modelHeader.dataSize = modelHeader.modelNameSize + sizeof(ModelFileHeader);

		struct SubmeshData{
			std::string name{};
			SubmeshFileHeader header{};
			std::array<std::string, kNumTextures> texFilename{};
		};
		struct MeshData{
			std::string name{};
			MeshFileHeader header{};
			BufferHandle meshDataBuffer{};
			std::vector<SubmeshData> submeshDataArray{};
		};
		std::vector<MeshData> meshDataArray(model.meshes.size());
		for(size_t i = 0; i < model.meshes.size(); i++) {
			const Mesh& mesh = *model.meshes[i];
			MeshData& meshData = meshDataArray[i];
			meshData.name = mesh.name;
			meshData.header.dataSize = mesh.meshData->GetDesc().byteSize + mesh.name.size();
			meshData.header.nameSize = mesh.name.size();
			meshData.header.positionSlot = mesh.positionStream.slot;
			meshData.header.positionOffset = mesh.positionStream.offset;
			meshData.header.normalSlot = mesh.normalStream.slot;
			meshData.header.normalOffset = mesh.normalStream.offset;
			meshData.header.texcoordSlot = mesh.uvStream.slot;
			meshData.header.texcoordOffset = mesh.uvStream.offset;
			meshData.header.tangentSlot = mesh.tangentStream.slot;
			meshData.header.tangentOffset = mesh.tangentStream.offset;
			meshData.header.indexFormat = mesh.indexBufferViews.format;
			meshData.header.indexOffset = mesh.indexBufferViews.offset;
			meshData.header.psoFlags = mesh.psoFlags;
			meshData.header.submeshCount = mesh.subMeshes.size();
			meshData.header.boundingBox = mesh.boundingBox;
			meshData.meshDataBuffer = mesh.meshData;

			for(const auto& [submeshName, submesh] : mesh.subMeshes) {
				SubmeshFileHeader submeshHeader{};
				submeshHeader.nameSize = submeshName.size();
				submeshHeader.indexCount = submesh.indexCount;
				submeshHeader.indexOffset = submesh.indexOffset;
				submeshHeader.vertexOffset = submesh.vertexOffset;
				submeshHeader.materialIndex = submesh.materialIndex;
				SubmeshData submeshData{};
				uint32_t filenameSize = 0;
				for(int j = 0; j < kNumTextures; j++) {
					submeshData.texFilename[j] = submesh.textures[j]->GetDesc().debugName;
					submeshHeader.texFilenameSizes[j] = submeshData.texFilename[j].size();
					filenameSize += submeshData.texFilename[j].size();
				}
				submeshData.name = submeshName;
				submeshData.header = submeshHeader;
				meshData.header.dataSize += sizeof(SubmeshFileHeader) + submeshHeader.nameSize + filenameSize;
				meshData.submeshDataArray.push_back(submeshData);
			}

			modelHeader.dataSize += sizeof(MeshFileHeader) + meshData.header.dataSize;
		}

		modelHeader.dataSize += model.materials.size() * sizeof(Material);

		// Model data layout:
		// [
		// 	ModelFileHeader,
		// 	model name,
		// 	[ 
		// 		MeshFileHeader, 
		// 		mesh name, 
		//		[ SubmeshFileHeader, submesh name ] ...
		// 		mesh data
		// 	] ...
		// 	[ MaterialData ] * materialCount
		// ]
		auto cmdList = s_GraphicsDevice->CreateCommandList(
			CommandListParameters().SetDebugName("Model Save Command List" + model.name));

		std::vector<char> modelData(modelHeader.dataSize);
		uint64_t offset = 0;
		auto addData = [&modelData, &offset](const void* data, size_t size) {
			memcpy(modelData.data() + offset, data, size);
			offset += size;
		};
		addData(&modelHeader, sizeof(ModelFileHeader));
		addData(model.name.data(), model.name.size());

		// 保存每一个 Mesh 的数据
		for(const MeshData& meshData : meshDataArray) {
			addData(&meshData.header, sizeof(MeshFileHeader));
			addData(meshData.name.data(), meshData.name.size());
			for (const auto& submeshData : meshData.submeshDataArray) {
				addData(&submeshData.header, sizeof(SubmeshFileHeader));
				addData(submeshData.name.data(), submeshData.name.size());
				for(int i = 0; i < kNumTextures; i++) {
					addData(submeshData.texFilename[i].data(), submeshData.texFilename[i].size());
				}
			}

			// 回读 Mesh 数据
			auto bufferDesc = meshData.meshDataBuffer->GetDesc();
			bufferDesc.SetCpuAccess(CpuAccessMode::Read)
				.SetDebugName("Read back texture " + bufferDesc.debugName);
			BufferHandle readBackMeshDataBuffer = s_GraphicsDevice->CreateBuffer(bufferDesc);
			cmdList->Open();
			cmdList->CopyBuffer(readBackMeshDataBuffer, 0, meshData.meshDataBuffer, 0, bufferDesc.byteSize);
			cmdList->Close();
			auto fenceVal = s_GraphicsDevice->ExecuteCommandList(cmdList);
			s_GraphicsDevice->QueueWaitForCommandList(CommandQueueType::Graphics, CommandQueueType::Graphics, fenceVal);

			void* mappedData = s_GraphicsDevice->MapBuffer(readBackMeshDataBuffer, CpuAccessMode::Read);
			addData(mappedData, bufferDesc.byteSize);
			s_GraphicsDevice->UnmapBuffer(readBackMeshDataBuffer);
		}

		std::vector<Material> materials(model.materials.size());
		for (size_t i = 0; i < model.materials.size(); ++i) {
			materials[i] = *model.materials[i];
		}
		addData(materials.data(), materials.size() * sizeof(Material));

		ofs.write(modelData.data(), modelData.size());
		ofs.close();

		return true;
	}

	std::shared_ptr<Model> LoadModelFromFile(const std::string& filename)
	{
		std::filesystem::path modelPath = s_ModelCacheDir + filename;
		std::ifstream ifs(modelPath, std::ios::binary);
		if (!ifs.is_open()) {
			return nullptr;
		}

		std::shared_ptr<Model> model = std::make_shared<Model>();

		auto cmdList = s_GraphicsDevice->CreateCommandList(
			CommandListParameters().SetDebugName("Model Load Command List" + model->name));
		cmdList->Open();

		// Read model data
		ModelFileHeader modelHeader{};
		ifs.read(reinterpret_cast<char*>(&modelHeader), sizeof(ModelFileHeader));
		std::vector<char> modelData(modelHeader.dataSize);
		ifs.read(modelData.data(), modelData.size());
		uint64_t offset = 0;
		auto copyData = [&modelData, &offset](void* dest, size_t size) {
			memcpy(dest, modelData.data() + offset, size);
			offset += size;
		};
		
		auto getData = [&modelData, &offset] <typename T> () {
			const T* dataPtr = reinterpret_cast<const T*>(modelData.data() + offset);
			offset += sizeof(T);
			return dataPtr;
		};
		model->name.resize(modelHeader.modelNameSize);
		copyData(model->name.data(), model->name.size());
		model->boundingBox = modelHeader.boundingBox;

		std::map<std::string_view, TextureHandle> uniqueTextureNames{};

		model->meshes.reserve(modelHeader.meshCount);
		model->materials.reserve(modelHeader.materialCount);
		// Read mesh data
		for (size_t i = 0; i < modelHeader.meshCount; ++i) {
			const MeshFileHeader* meshHeader = getData.operator()<MeshFileHeader>();

			uint64_t meshBeginOffset = offset;
			
			std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
			mesh->name.resize(meshHeader->nameSize);
			copyData(mesh->name.data(), mesh->name.size());
			mesh->boundingBox = meshHeader->boundingBox;

			for (size_t j = 0; j < meshHeader->submeshCount; ++j) {
				const SubmeshFileHeader* submeshHeader = getData.operator()<SubmeshFileHeader>();
				
				Mesh::SubMesh submesh{};
				submesh.indexCount = submeshHeader->indexCount;
				submesh.indexOffset = submeshHeader->indexOffset;
				submesh.materialIndex = submeshHeader->materialIndex;
				submesh.vertexOffset = submesh.vertexOffset;

				std::string submeshName{};
				submeshName.resize(submeshHeader->nameSize);
				copyData(submeshName.data(), submeshName.size());
				submesh.textures.resize(kNumTextures);
				for(int k = 0; k < kNumTextures; k++){
					std::string_view textureName = std::string_view(modelData.data() + offset, submeshHeader->texFilenameSizes[k]);
					offset += submeshHeader->texFilenameSizes[k];

					if(s_CommonTextures[k]->GetDesc().debugName == textureName){
						submesh.textures[k] = s_CommonTextures[k];
					}
					else if(!uniqueTextureNames.contains(textureName)){
						submesh.textures[k] = TextureManager::LoadTextureFromFile(std::string(textureName));
						uniqueTextureNames[textureName] = submesh.textures[k];
					}
					else {
						submesh.textures[k] = uniqueTextureNames[textureName];
					}
				}
				mesh->subMeshes.emplace(submeshName, std::move(submesh));
			}

			auto bufferSize = meshBeginOffset + meshHeader->dataSize - offset;
			mesh->meshData = s_GraphicsDevice->CreateBuffer(BufferDesc()
				.SetByteSize(bufferSize)
				.SetDebugName("Mesh Data " + mesh->name));
			cmdList->WriteBuffer(mesh->meshData, modelData.data() + offset, bufferSize);
			offset += bufferSize;

			mesh->psoFlags = meshHeader->psoFlags;
			auto addVertexBufferBinding = [&mesh](auto& binding, PSOFlags flag, uint32_t slot, uint64_t offset) {
				if(HasFlags(PSOFlags(mesh->psoFlags), flag)){
					binding = VertexBufferBinding{mesh->meshData, slot, offset};
				}
			};
			addVertexBufferBinding(mesh->positionStream, kHasPosition, meshHeader->positionSlot, meshHeader->positionOffset);
			addVertexBufferBinding(mesh->normalStream, kHasNormal, meshHeader->normalSlot, meshHeader->normalOffset);
			addVertexBufferBinding(mesh->uvStream, kHasUV, meshHeader->texcoordSlot, meshHeader->texcoordOffset);
			addVertexBufferBinding(mesh->tangentStream, kHasTangent, meshHeader->tangentSlot, meshHeader->tangentOffset);

			mesh->indexBufferViews = IndexBufferBinding{mesh->meshData, meshHeader->indexFormat, meshHeader->indexOffset};

			model->meshes.push_back(mesh);
		}

		// Read material data
		for(size_t i = 0; i < modelHeader.materialCount; ++i){
			std::shared_ptr<Material> material = std::make_shared<Material>();
			copyData(material.get(), sizeof(Material));
			model->materials.push_back(material);
		}

		if (model->materials.size() > 0) {
			auto matByteSize = Math::Align(sizeof(Material), size_t(c_ConstantBufferOffsetSizeAlignment));
			std::vector<uint8_t> materialData(matByteSize * model->materials.size());
			for (std::size_t i = 0; i < model->materials.size(); i++) {
				memcpy(materialData.data() + i * matByteSize, model->materials[i].get(), sizeof(Material));
			}
			model->materialData = s_GraphicsDevice->CreateBuffer(BufferDesc().
				SetByteSize(materialData.size()).
				SetIsConstantBuffer(true).
				SetDebugName("Model MaterialData" + model->name));
			cmdList->WriteBuffer(model->materialData, materialData.data(), materialData.size());
		}

		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);

		ifs.close();

		return model;
	}

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
    void CreateMesh(Mesh& mesh, const std::span<MeshData>& meshDatas);

	std::shared_ptr<Model> LoadModelFromGeometry(
		const std::string& name,
		const Geometry::GeometryMesh& geometryMesh,
		std::shared_ptr<Material> material)
	{
		if (geometryMesh.vertices.empty()){
			return nullptr;
		}

		auto model = std::make_shared<Model>();
		model->name = name;
		if(material == nullptr){
			model->materials.emplace_back(std::make_shared<Material>());
		}
		else{
			model->materials.push_back(material);
		}
		auto& mesh = model->meshes.emplace_back(std::make_shared<Mesh>());
		mesh->name = name;

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

		CreateMesh(*mesh, {&meshData, 1});
		model->materialData = s_GraphicsDevice->CreateBuffer(BufferDesc()
			.SetByteSize(Math::Align(sizeof(Material), size_t(c_ConstantBufferOffsetSizeAlignment)))
			.SetIsConstantBuffer(true)
			.SetDebugName("Model MaterialData" + name));

		auto cmdList = s_GraphicsDevice->CreateCommandList(
			CommandListParameters().SetDebugName("InitMaterialData"));
		cmdList->Open();
		cmdList->WriteBuffer(model->materialData, model->materials[0].get(), sizeof(Material));
		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);

		mesh->subMeshes[mesh->name].textures = {
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kWhiteOpaque2D),
			TextureManager::GetDefaultTexture(TextureManager::kBlackTransparent2D),
			TextureManager::GetDefaultTexture(TextureManager::kDefaultNormalTex)
		};

		model->boundingBox = mesh->boundingBox;

		return model;
	}

    std::shared_ptr<Model> LoadModel(const std::string &filename)
    {
		auto model = std::make_shared<Model>();

		if(std::filesystem::exists(s_ModelCacheDir + filename)) {
			model = LoadModelFromFile(filename);
		}
		else {
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

			Math::AxisAlignedBox boundingBox{};
			for (const auto& mesh : model->meshes) {
				boundingBox = Math::AxisAlignedBox::Union(boundingBox, mesh->boundingBox);
			}
			model->boundingBox = boundingBox;

			SaveModelToFile(*model, filename);
		}

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

		const auto& aabb = mesh->mAABB;
		Math::Vector3 boxMin{aabb.mMin.x, aabb.mMin.y, aabb.mMin.z};
		Math::Vector3 boxMax{aabb.mMax.x, aabb.mMax.y, aabb.mMax.z};
		meshData.boundingBox = Math::AxisAlignedBox{boxMin, boxMax};

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
			uvs.insert(uvs.end(), meshData.texcoords.begin(), meshData.texcoords.end());
			tangents.insert(tangents.end(), meshData.tangents.begin(), meshData.tangents.end());
			indices.insert(indices.end(), meshData.indices.begin(), meshData.indices.end());

			mesh.boundingBox = Math::AxisAlignedBox::Union(mesh.boundingBox, meshData.boundingBox);
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

		assert(indices.size() > 0);
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
		std::map<std::string, TextureHandle> uniqueTextures{};

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

			auto& srcHandle = matTextures[i];

			auto tryCreateTexture = [&](aiTextureType type) {
				MaterialTex materialTex;
				switch (type) {
					case aiTextureType_DIFFUSE : 
					case aiTextureType_BASE_COLOR: materialTex = kBaseColor; break;
					case aiTextureType_DIFFUSE_ROUGHNESS: materialTex = kDiffuseRoughness; break;
					case aiTextureType_METALNESS: materialTex = kMetalness; break;
					case aiTextureType_AMBIENT_OCCLUSION : materialTex = kOcclusion; break;
					case aiTextureType_EMISSIVE: materialTex = kEmissive; break;
					case aiTextureType_NORMALS: materialTex = kNormal; break;
					default: materialTex = kBaseColor; break;
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

		auto matByteSize = Math::Align(sizeof(Material), size_t(c_ConstantBufferOffsetSizeAlignment));
		std::vector<uint8_t> materialData(matByteSize * model.materials.size());
		for (std::size_t i = 0; i < model.materials.size(); i++) {
			memcpy(materialData.data() + i * matByteSize, model.materials[i].get(), sizeof(Material));
		}
		model.materialData = s_GraphicsDevice->CreateBuffer(BufferDesc().
			SetByteSize(materialData.size()).
			SetIsConstantBuffer(true).
			SetDebugName("Model MaterialData" + model.name));

		auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("Init MaterialConstants"));
		cmdList->Open();
		cmdList->WriteBuffer(model.materialData, materialData.data(), materialData.size());
		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);
	}
	
}

#pragma once
#ifndef __MODEL_H__
#define __MODEL_H__

#include "Runtime/Render/Material.h"
#include "Shaders/ForwardShader/ResourceData.h"

struct aiScene;
struct aiNode;

namespace DSM {
    class MeshRenderer;
    class Material;
    class Mesh;

    namespace Geometry {
        struct GeometryMesh;
    }

    struct Model
    {
        std::string name{};
        std::string filePath{};
        std::vector<std::shared_ptr<Mesh>> meshes{};
        std::vector<std::vector<uint32_t>> meshMaterialIndices{};
        std::vector<std::shared_ptr<Material>> materials{};
        Math::AxisAlignedBox boundingBox{};

        static std::shared_ptr<Model> LoadModel(const std::string& filename);
        static std::shared_ptr<Model> LoadModelFromGeometry(
            const std::string& name, 
            Geometry::GeometryMesh geometryMesh,
            std::shared_ptr<Material> material = nullptr);

        static void Create(IDevice* device);
        static void Destroy();

    private:
		static void ProcessMaterial(Model& model, const std::string& filename, const aiScene* scene);
		static void ProcessNode(Model& model, const aiScene* scene);
        static std::pair<std::shared_ptr<Mesh>, std::vector<uint32_t>> GetMeshFromNode(const aiNode* node, const aiScene* scene);

    private:
        inline static IDevice* sm_Device{};
    	inline static std::array<TextureHandle, ShaderResource::kNumTextures> sm_CommonTextures{};
        inline static std::unordered_map<std::string, std::shared_ptr<Model>> sm_ModelCache{};
    };

}

#endif

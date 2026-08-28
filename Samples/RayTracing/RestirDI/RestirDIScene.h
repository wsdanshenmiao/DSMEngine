#pragma once

#include "RestirDIAliasTable.h"
#include "RestirDISettings.h"
#include "Runtime/Graphics/DSMRHI.h"
#include "Runtime/Framework/Scene.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace DSM {
    class Material;
    struct Mesh;
}

namespace DSM::RestirDI {

    struct SceneSyncResult
    {
        bool topologyRebuilt = false;
        bool transformsUpdated = false;
        bool lightDistributionUpdated = false;
        bool materialDistributionUpdated = false;
        bool historyResetRequired = false;
    };

    class SceneAdapter final
    {
    public:
        SceneAdapter() = default;
        ~SceneAdapter() = default;

        SceneSyncResult Synchronize(IDevice* device, const Settings& settings, std::string& error);
        void RecordBuildAndUpload(ICommandList* commandList);
        void Reset();

        [[nodiscard]] RT::IAccelStruct* GetTLAS() const noexcept { return m_TLAS; }
        [[nodiscard]] IBuffer* GetVertexBuffer() const noexcept { return m_VertexBuffer; }
        [[nodiscard]] IBuffer* GetIndexBuffer() const noexcept { return m_IndexBuffer; }
        [[nodiscard]] IBuffer* GetGeometryBuffer() const noexcept { return m_GeometryBuffer; }
        [[nodiscard]] IBuffer* GetInstanceBuffer() const noexcept { return m_InstanceBuffer; }
        [[nodiscard]] IBuffer* GetMaterialBuffer() const noexcept { return m_MaterialBuffer; }
        [[nodiscard]] IBuffer* GetLightBuffer() const noexcept { return m_LightBuffer; }
        [[nodiscard]] IBuffer* GetLightAliasBuffer() const noexcept { return m_LightAliasBuffer; }
        [[nodiscard]] IBuffer* GetEmissiveBuffer() const noexcept { return m_EmissiveBuffer; }
        [[nodiscard]] IBuffer* GetEmissiveAliasBuffer() const noexcept { return m_EmissiveAliasBuffer; }

        [[nodiscard]] uint32_t GetLogicalInstanceCount() const noexcept { return m_LogicalInstanceCount; }
        [[nodiscard]] uint32_t GetLightCount() const noexcept { return m_LightCount; }
        [[nodiscard]] uint32_t GetEmissiveCount() const noexcept { return m_EmissiveCount; }
        [[nodiscard]] float GetAnalyticPower() const noexcept { return m_AnalyticAlias.totalWeight; }
        [[nodiscard]] float GetEmissivePower() const noexcept { return m_EmissiveAlias.totalWeight; }
        [[nodiscard]] const std::vector<TextureHandle>& GetTextures() const noexcept { return m_Textures; }

    private:
        struct BlasRecord
        {
            std::shared_ptr<Mesh> mesh{};
            RT::AccelStructHandle accelerationStructure{};
            HeapHandle heap{};
            // RHI 当前不应用 GeometryTriangles 的 vertex/index offset，
            // 因此每个 Geometry 使用从零开始的专属 AS 构建缓冲。
            std::vector<BufferHandle> vertexBuffers{};
            std::vector<BufferHandle> indexBuffers{};
            // vertexBase、indexOffset、indexCount、vertexCount。
            std::vector<GpuUint4> buildRanges{};
            std::vector<RT::GeometryDesc> geometries{};
        };

        void GatherScene(const Settings& settings);
        void GatherLights();
        void RefreshEmissiveDistribution();
        void RecreateSceneResources(IDevice* device);
        void RecreateDistributionResources(IDevice* device);
        void UpdateTransforms();
        void BuildTLASInstances();

        [[nodiscard]] uint64_t CalculateTopologyHash(const Settings& settings) const;
        [[nodiscard]] uint64_t CalculateTransformHash() const;
        [[nodiscard]] uint64_t CalculateLightHash() const;
        [[nodiscard]] uint64_t CalculateLightDistributionHash() const;

    private:
        std::vector<GpuVertex> m_Vertices{};
        std::vector<uint32_t> m_Indices{};
        std::vector<GpuGeometry> m_Geometries{};
        std::vector<GpuInstance> m_Instances{};
        std::vector<GpuMaterial> m_Materials{};
        std::vector<GpuAnalyticLight> m_Lights{};
        std::vector<GpuEmissiveTriangle> m_EmissiveTriangles{};
        AliasTable m_AnalyticAlias{};
        AliasTable m_EmissiveAlias{};
        std::vector<TextureHandle> m_Textures{};
        std::vector<RT::InstanceDesc> m_TLASInstances{};
        std::vector<BlasRecord> m_BLASRecords{};
        std::vector<uint32_t> m_InstanceBLASIndices{};
        std::vector<uint32_t> m_StableInstanceIDs{};
        std::vector<std::shared_ptr<Mesh>> m_InstanceMeshes{};
        std::vector<Math::Matrix4> m_CurrentTransforms{};
        std::vector<uint32_t> m_InstanceMasks{};
        std::vector<RT::InstanceFlags> m_InstanceFlags{};

        BufferHandle m_VertexBuffer{};
        BufferHandle m_IndexBuffer{};
        BufferHandle m_GeometryBuffer{};
        BufferHandle m_InstanceBuffer{};
        BufferHandle m_MaterialBuffer{};
        BufferHandle m_LightBuffer{};
        BufferHandle m_LightAliasBuffer{};
        BufferHandle m_EmissiveBuffer{};
        BufferHandle m_EmissiveAliasBuffer{};
        RT::AccelStructHandle m_TLAS{};
        HeapHandle m_TLASHeap{};

        std::unordered_map<uint32_t, GpuMatrix> m_PreviousTransforms{};
        uint64_t m_TopologyHash = 0;
        uint64_t m_TransformHash = 0;
        uint64_t m_LightHash = 0;
        uint64_t m_LightDistributionHash = 0;
        uint32_t m_LogicalInstanceCount = 0;
        uint32_t m_LightCount = 0;
        uint32_t m_EmissiveCount = 0;
        bool m_NeedsFullBuild = false;
        bool m_NeedsTLASUpdate = false;
        bool m_NeedsDistributionUpload = false;
        bool m_FirstSync = true;
    };

}

#pragma once
#ifndef __RAYTRACING_H__
#define __RAYTRACING_H__

#include "GraphicsCommon.h"

namespace DSM {
    // 前向声明，避免与 Shader.h、ResourceBindings.h 和 Buffer.h 形成包含环。
    class IBuffer;
    class IShaderLibrary;
    class IBindingLayout;
    class IBindingSet;

    // DirectX Raytracing（DXR）抽象层。
    // 命名空间 RT 对应 NVRHI 的 nvrhi::rt，提供加速结构、管线状态和着色器表。
    namespace RT {
        // 句柄需在描述结构中使用，因此先前向声明核心对象。
        struct AccelStructDesc;
        struct PipelineDesc;
        struct ShaderTableDesc;
        class IAccelStruct;
        class IRayTracingPipeline;
        class IShaderTable;
        using AccelStructHandle = RefPtr<IAccelStruct>;
        using RayTracingPipelineHandle = RefPtr<IRayTracingPipeline>;
        using ShaderTableHandle = RefPtr<IShaderTable>;

        // 加速结构构建标志（对应 D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS）。
        enum class AccelStructBuildFlags : uint32_t
        {
            None            = 0,
            AllowUpdate     = 0x1,
            AllowCompaction = 0x2,
            PreferFastTrace = 0x4,
            PreferFastBuild = 0x8,
            MinimizeMemory  = 0x10,
            PerformUpdate   = 0x20,
        };

        // 实例标志（对应 D3D12_RAYTRACING_INSTANCE_FLAGS）。
        enum class InstanceFlags : uint32_t
        {
            None                         = 0,
            ForceOpaque                  = 0x1,
            ForceNonOpaque               = 0x2,
            TriangleCullDisable          = 0x4,
            TriangleFrontCounterClockwise = 0x8,
        };

        // 三角形几何体描述（用于 BLAS 构建）。
        struct GeometryDesc
        {
            IBuffer* vertexBuffer = nullptr;
            uint32_t vertexStride = 0;
            uint32_t vertexCount = 0;
            uint32_t vertexOffset = 0;
            Format vertexFormat = Format::RGB32_FLOAT;

            bool isIndexed = false;
            IBuffer* indexBuffer = nullptr;
            uint32_t indexCount = 0;
            uint32_t indexOffset = 0;
            Format indexFormat = Format::R32_UINT;

            // 可选的 3x4 行主序变换矩阵地址。
            IBuffer* transformBuffer = nullptr;
            uint32_t transformOffset = 0;
            bool isOpaque = true;
        };

        // 实例描述（用于 TLAS 构建）。
        struct InstanceDesc
        {
            AccelStructHandle bottomLevelAS;
            float transform[12] = { 1,0,0,0, 0,1,0,0, 0,0,1,0 };
            uint32_t instanceID = 0;
            uint32_t instanceMask = 0xFF;
            uint32_t instanceContributionToHitGroupIndex = 0;
            InstanceFlags flags = InstanceFlags::None;
        };

        // 加速结构描述。
        struct AccelStructDesc
        {
            bool isTopLevel = false;
            AccelStructBuildFlags buildFlags = AccelStructBuildFlags::None;
            std::vector<GeometryDesc> geometries;
            uint32_t instanceCount = 0;
            std::string debugName;
        };

        // 管线中的单个着色器导出（raygen / miss / closest-hit 等）。
        struct PipelineShaderDesc
        {
            IShaderLibrary* shader = nullptr;
            std::string exportName;
            IBindingLayout* bindingLayout = nullptr;
        };

        // 命中组：将 closest-hit、any-hit 与 intersection 组合。
        struct HitGroupDesc
        {
            std::string hitGroupName;
            std::string closestHitShader;
            std::string anyHitShader;
            std::string intersectionShader;
            IBindingLayout* bindingLayout = nullptr;
        };

        // 光线追踪管线描述，复用 CreateShaderLibrary 产物与导出名。
        struct PipelineDesc
        {
            std::vector<PipelineShaderDesc> shaders;
            std::vector<HitGroupDesc> hitGroups;
            uint32_t maxRecursionDepth = 1;
            uint32_t maxPayloadSize = 0;
            uint32_t maxAttributeSize = 0;
            std::vector<IBindingLayout*> globalBindingLayouts;
            std::string debugName;
        };

        // 着色器表描述，对齐 NVRHI rt::ShaderTableDesc。
        struct ShaderTableDesc
        {
            RayTracingPipelineHandle pipeline;
            std::string rayGenerationShader;
            std::vector<std::string> missShaders;
            std::vector<std::string> hitGroups;
            std::vector<std::string> callableShaders;

            // 各段对应的本地绑定集合，可为空。
            std::vector<IBindingSet*> rayGenerationBindings;
            std::vector<IBindingSet*> missBindings;
            std::vector<IBindingSet*> hitGroupBindings;
            std::vector<IBindingSet*> callableBindings;
        };

        struct DispatchRaysArguments
        {
            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;
        };

        // SetRayTracingState 使用的状态。
        struct State
        {
            ShaderTableHandle shaderTable;
            std::vector<IBindingSet*> bindingSets;
        };

        class IAccelStruct : public IResource
        {
        public:
            [[nodiscard]] virtual const AccelStructDesc& GetDesc() const = 0;
            [[nodiscard]] virtual uint64_t GetDeviceAddress() const = 0;
            [[nodiscard]] virtual IBuffer* GetDataBuffer() const = 0;
            virtual bool IsCompacted() const = 0;
            virtual bool IsTopLevel() const = 0;
        };

        class IRayTracingPipeline : public IResource
        {
        public:
            [[nodiscard]] virtual const PipelineDesc& GetDesc() const = 0;
        };

        class IShaderTable : public IResource
        {
        public:
            [[nodiscard]] virtual const ShaderTableDesc& GetDesc() const = 0;
            [[nodiscard]] virtual IRayTracingPipeline* GetPipeline() const = 0;
            [[nodiscard]] virtual uint32_t GetNumEntries() const = 0;
        };
    } // namespace RT

    ENABLE_ENUM_BIT_OPERATOR(RT::AccelStructBuildFlags)
    ENABLE_ENUM_BIT_OPERATOR(RT::InstanceFlags)
} // namespace DSM

#endif

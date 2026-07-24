#pragma once
#ifndef __D3D12_RAYTRACING_H__
#define __D3D12_RAYTRACING_H__

#include "Runtime/Graphics/D3D12.h"
#include "Runtime/Graphics/RayTracing.h"
#include "Runtime/Graphics/D3D12/D3D12Common.h"
#include "Runtime/Graphics/Buffer.h"
#include "D3D12-Buffer.h"
#include "D3D12-ResourceBindings.h"
#include <unordered_map>
#include <vector>

namespace DSM::D3D12 {

    class Device;
    class RootSignature;
    class CommandList;

    // 标志位转换（供 Device / CommandList 复用）
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ConvertAccelStructBuildFlags(RT::AccelStructBuildFlags flags);
    D3D12_RAYTRACING_INSTANCE_FLAGS ConvertInstanceFlags(RT::InstanceFlags flags);

    // 加速结构（对齐 NVRHI d3d12::AccelStruct）
    // 持有 dataBuffer（结果数据）+ 预构建信息 + 描述，采用堆绑定模型由外部 BindAccelStructMemory 放置。
    class AccelStruct : public RT::IAccelStruct
    {
    public:
        AccelStruct(const Context& context);
        ~AccelStruct() override = default;

        // 预计算构建信息并创建虚拟 dataBuffer（尚未绑定到堆）
        bool Finalize(const RT::AccelStructDesc& desc, Device* device);

        const RT::AccelStructDesc& GetDesc() const override { return m_Desc; }
        uint64_t GetDeviceAddress() const override;
        bool IsCompacted() const override { return m_Compacted; }
        bool IsTopLevel() const override { return m_Desc.isTopLevel; }

        Buffer* GetDataBuffer() const override { return m_DataBuffer; }
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& GetPrebuildInfo() const { return m_PrebuildInfo; }

        Object GetNativeObject(ObjectType type) override;

    private:
        const Context& m_Context;
        RT::AccelStructDesc m_Desc{};
        RefPtr<Buffer> m_DataBuffer{};
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO m_PrebuildInfo{};
        bool m_Compacted = false;
    };

    // 光线追踪管线状态对象（对齐 NVRHI d3d12::RayTracingPipeline）
    // 持有 ID3D12StateObject + 导出表 + 全局根签名（v1 不含本地根签名）。
    class RayTracingPipeline : public RT::IRayTracingPipeline
    {
    public:
        struct ExportEntry
        {
            IBindingLayout* bindingLayout = nullptr;
            const void* identifier = nullptr;
        };

        RayTracingPipeline(const Context& context, Device* device);
        ~RayTracingPipeline() override = default;

        bool Finalize(const RT::PipelineDesc& desc);

        const RT::PipelineDesc& GetDesc() const override { return m_Desc; }

        const ExportEntry* GetExport(const char* name) const;
        uint32_t GetShaderIdentifierSize() const { return m_ShaderIdentifierSize; }
        RootSignature* GetGlobalRootSignature() const { return m_GlobalRootSignature; }
        ID3D12StateObject* GetStateObject() const { return m_StateObject; }

        Object GetNativeObject(ObjectType type) override;

    private:
        const Context& m_Context;
        Device* m_Device;
        RT::PipelineDesc m_Desc{};
        RefPtr<ID3D12StateObject> m_StateObject{};
        RefPtr<ID3D12StateObjectProperties> m_StateObjectInfo{};
        std::unordered_map<std::string, ExportEntry> m_Exports{};
        RefPtr<RootSignature> m_GlobalRootSignature{};
        uint32_t m_ShaderIdentifierSize = 0;
    };

    // 着色器表（对齐 NVRHI d3d12::ShaderTable）
    // 持有各段导出（标识符 + 本地绑定），烘焙时写入上传缓冲并缓存 D3D12_DISPATCH_RAYS_DESC。
    class ShaderTable : public RT::IShaderTable
    {
    public:
        struct Entry
        {
            const void* identifier = nullptr;
            IBindingSet* bindings = nullptr;
        };

        ShaderTable(RayTracingPipeline* pipeline, const RT::ShaderTableDesc& desc);
        ~ShaderTable() override = default;

        const RT::ShaderTableDesc& GetDesc() const override { return m_Desc; }
        RT::IRayTracingPipeline* GetPipeline() const override { return m_Pipeline; }
        uint32_t GetNumEntries() const override { return m_NumEntries; }

        // 将着色器表烘焙到命令列表的上传缓冲，并填充 DispatchRays 描述
        void Bake(CommandList& cmdList, D3D12_DISPATCH_RAYS_DESC& outDesc) const;

    private:
        RefPtr<RayTracingPipeline> m_Pipeline;
        RT::ShaderTableDesc m_Desc{};
        Entry m_RayGeneration{};
        std::vector<Entry> m_MissShaders{};
        std::vector<Entry> m_HitGroups{};
        std::vector<Entry> m_CallableShaders{};
        uint32_t m_NumEntries = 0;
    };

} // namespace DSM::D3D12

#endif

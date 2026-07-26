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

    class AccelStruct : public RT::IAccelStruct
    {
    public:
        AccelStruct(const Context& context);
        ~AccelStruct() override = default;

        void Create(const RT::AccelStructDesc& desc, Device* device);
        void CreateSRV(size_t descriptor) const;

        Object GetNativeObject(ObjectType type) override;

        inline const RT::AccelStructDesc& GetDesc() const override { return m_Desc; }
        inline uint64_t GetDeviceAddress() const override { return dataBuffer != nullptr ? dataBuffer->GetGpuVirtualAddress() : 0; }

    public:
        RefPtr<Buffer> dataBuffer{};
        std::vector<RT::AccelStructHandle> bottomLevelASes{};
        std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs{};

    private:
        const Context& m_Context;
        RT::AccelStructDesc m_Desc{};
    };

    class RayTracingPipeline : public RT::IPipeline
    {
    public:
        struct ExportEntry
        {
            IBindingLayout* bindingLayout = nullptr;
            const void* identifier = nullptr;
        };

        RayTracingPipeline(const Context& context);
        ~RayTracingPipeline() override = default;

        const ExportEntry* GetExport(const char* name) const;
        uint32_t GetShaderTableEntrySize() const;

        const RT::PipelineDesc& GetDesc() const override { return m_Desc; }
        RT::ShaderTableHandle CreateShaderTable() override;

    private:
        const Context& m_Context;
        RT::PipelineDesc m_Desc{};

        RefPtr<RootSignature> m_GlobalRootSignature{};
        std::unordered_map<IBindingLayout*, RefPtr<RootSignature>> m_LocalRootSignatures{};

        RefPtr<ID3D12StateObject> m_PipelineState{};
        RefPtr<ID3D12StateObjectProperties> m_StateObjectInfo{};

        std::unordered_map<std::string, ExportEntry> m_Exports{};

        uint32_t m_MaxLocalRootSize = 0;
    };

    class ShaderTable : public RT::IShaderTable
    {
    public:
        struct Entry
        {
            const void* identifier = nullptr;
            BindingSetHandle bindings = nullptr;
        };

        ShaderTable(const Context& context, RayTracingPipeline* pipeline);
        ~ShaderTable() override = default;

        uint32_t GetNumEntries() const { return 1 + m_MissShaders.size() + m_HitGroups.size() + m_CallableShaders.size(); }

        void SetGenerationShader(const char* exportName, IBindingSet* bindingSet = nullptr) override;
        int AddMissShader(const char* exportName, IBindingSet* bindingSet = nullptr) override;
        int AddHitGroup(const char* exportName, IBindingSet* bindingSet = nullptr) override;
        int AddCallableShader(const char* exportName, IBindingSet* bindingSet = nullptr) override;
        void ClearMissShaders() override;
        void ClearHitGroups() override;
        void ClearCallableShaders() override;

        inline RT::IPipeline* GetPipeline() const override { return m_Pipeline; }

    private:
        bool VerifyEntry(const RayTracingPipeline::ExportEntry* exportEntry, IBindingSet* bindingSet) const;
        int AddEntry(const char* exportName, IBindingSet* bindingSet, std::vector<Entry>& entryList);

    private:
        const Context& m_Context;
        RefPtr<RayTracingPipeline> m_Pipeline{};

        Entry m_RayGeneration{};
        std::vector<Entry> m_MissShaders{};
        std::vector<Entry> m_HitGroups{};
        std::vector<Entry> m_CallableShaders{};
        
        uint32_t m_Version = 0;
    };

    class ShaderTableState
    {
    public:
        uint32_t committedVersion = 0;
        ID3D12DescriptorHeap* descriptorHeapSRV = nullptr;
        ID3D12DescriptorHeap* descriptorHeapSampler = nullptr;
        D3D12_DISPATCH_RAYS_DESC dispatchRaysTemplate = {};
    };

} // namespace DSM::D3D12

#endif

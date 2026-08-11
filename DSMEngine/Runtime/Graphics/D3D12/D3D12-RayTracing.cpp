#include "D3D12-RayTracing.h"
#include "D3D12-Device.h"
#include "D3D12-CommandList.h"
#include <cstring>
#include <format>

namespace DSM::D3D12 {
    AccelStruct::AccelStruct(const Context& context)
        : m_Context(context) {}

    void AccelStruct::Create(const RT::AccelStructDesc &desc, Device* device)
    { 
        m_Desc = desc;

        if (m_Context.device5 == nullptr || device == nullptr)
            return;
        
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs{};
        auto buildInputs = GetAccelerationStructureBuildInputs(m_Desc, geometryDescs);
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
        m_Context.device5->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

        auto buffer = device->CreateBuffer(BufferDesc()
            .SetCanHaveUAVs(true)
            .SetKeepInitialState(true)
            .SetIsAccelStructStorage(true)
            .SetIsAccelStructStorage(true)
            .SetDebugName(m_Desc.debugName)
            .SetIsVirtual(m_Desc.isVirtual)
            .SetByteSize(prebuildInfo.ResultDataMaxSizeInBytes)
            .SetInitialState(m_Desc.isTopLevel ? ResourceStates::AccelStructRead : ResourceStates::AccelStructBuildBlas));
        dataBuffer = Utility::CheckedCast<Buffer*>(buffer.Get());

        for(auto& geometry : m_Desc.bottomLevelGeometries) {
            geometry.geometryData.triangles.indexBuffer = nullptr;
            geometry.geometryData.triangles.vertexBuffer = nullptr;
        }
    }

    void AccelStruct::CreateSRV(size_t descriptor) const
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = dataBuffer->GetGpuVirtualAddress();
    
        m_Context.device->CreateShaderResourceView(nullptr, &srvDesc, { descriptor });
    }

    Object AccelStruct::GetNativeObject(ObjectType type)
    {
        if (dataBuffer != nullptr)
            return dataBuffer->GetNativeObject(type);
        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////
    // RayTracingPipeline
    //////////////////////////////////////////////////////////////////////////
    RayTracingPipeline::RayTracingPipeline(const Context& context, Device* device)
        : m_Context(context), m_Device(device) {}

    const RayTracingPipeline::ExportEntry* RayTracingPipeline::GetExport(const char* name) const
    {
        auto it = m_Exports.find(name);
        return it == m_Exports.end() ? nullptr : &it->second;
    }

    uint32_t RayTracingPipeline::GetShaderTableEntrySize() const
    {
        uint32_t requiredSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(uint64_t) * m_MaxLocalRootSignatureSize;
        return Math::Align(requiredSize, uint32_t(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    }

    RT::ShaderTableHandle RayTracingPipeline::CreateShaderTable(const RT::ShaderTableDesc& desc)
    {
        BufferHandle cache{};
        if (desc.isCached)
        {
            if (desc.maxEntries == 0)
            {
                m_Context.Error("maxEntries must be nonzero for a cached ShaderTable");
                return nullptr;
            }

            cache = m_Device->CreateBuffer(BufferDesc()
                .SetDebugName(desc.debugName)
                .SetByteSize(GetShaderTableEntrySize() * desc.maxEntries)
                .SetIsShaderBindingTable(true)
                .SetInitialState(ResourceStates::ShaderResource));
            if (cache == nullptr)
                return nullptr;
        }

        auto shaderTable = new ShaderTable(m_Context, this, desc);
        shaderTable->cache = cache;
        return RT::ShaderTableHandle(shaderTable);
    }

    //////////////////////////////////////////////////////////////////////////
    // ShaderTable
    //////////////////////////////////////////////////////////////////////////
    ShaderTable::ShaderTable(const Context& context, RayTracingPipeline* pipeline, const RT::ShaderTableDesc& desc)
        : m_Context(context), m_Desc(desc), m_Pipeline(pipeline) {}

    bool ShaderTable::IsStateValid(const ShaderTableState& state, const DeviceResources& resources) const
    {
        bool versionMatch = m_Version == state.committedVersion;
        if (m_Pipeline->HasLocalResources()) {
            return versionMatch &&
                state.descriptorHeapSRV == resources.shaderResourceViewHeap.GetShaderVisibleHeap() &&
                state.descriptorHeapSampler == resources.samplerHeap.GetShaderVisibleHeap();
        }

        return versionMatch;
    }

    void ShaderTable::Bake(uint8_t* cpuVA, D3D12_GPU_VIRTUAL_ADDRESS gpuVA, DeviceResources& resources, ShaderTableState& state)
    {
        if(cpuVA == nullptr || gpuVA == 0)
        {
            m_Context.Error("Invalid CPU or GPU virtual address for baking shader table");
            return;
        }

        // 必须取最大的 shader record 的大小
        const uint32_t entrySize = m_Pipeline->GetShaderTableEntrySize();
        auto writeEntry = [this, &resources, &cpuVA, &gpuVA, entrySize](const Entry& entry) {
            if(entry.identifier == nullptr)
            {
                m_Context.Error("Shader table entry has no shader identifier");
                return;
            }

            // 写入 shader identifier
            std::memcpy(cpuVA, entry.identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

            // 若有局部根签名，写入 shader table
            if (entry.bindings != nullptr) {
                auto* bindingSet = Utility::CheckedCast<BindingSet*>(entry.bindings.Get());
                auto* layout = Utility::CheckedCast<BindingLayout*>(bindingSet->GetLayout());

                if (layout->descriptorTableSizeSamplers > 0) {
                    auto* table = reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(cpuVA
                        + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + layout->rootParameterIndexSamplers * sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
                    // 将 descriptor table 的 GPU handle 写入 shader table
                    *table = resources.samplerHeap.GetGpuHandle(bindingSet->descriptorIndexSamplers);
                }

                if (layout->descriptorTableSizeSRVs > 0) {
                    auto* table = reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(cpuVA
                        + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + layout->rootParameterIndexSRVs * sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
                    // 将 descriptor table 的 GPU handle 写入 shader table
                    *table = resources.shaderResourceViewHeap.GetGpuHandle(bindingSet->descriptorIndexSRVs);
                }

                if (!layout->rootParametersVolatileCBs.empty()){
                    m_Context.Error("Cannot use volatile constant buffers in a shader binding table");
                }
            }

            cpuVA += entrySize;
            gpuVA += entrySize;
        };

        auto& dispatchDesc = state.dispatchRaysTemplate;
        std::memset(&dispatchDesc, 0, sizeof(dispatchDesc));

        // 第一个 shader record 放置 ray generation shader
        dispatchDesc.RayGenerationShaderRecord.StartAddress = gpuVA;
        dispatchDesc.RayGenerationShaderRecord.SizeInBytes = entrySize;
        writeEntry(m_RayGeneration);

        // 写入 miss shader
        if (!m_MissShaders.empty()) {
            dispatchDesc.MissShaderTable.StartAddress = gpuVA;
            dispatchDesc.MissShaderTable.StrideInBytes = m_MissShaders.size() == 1 ? 0 : entrySize;
            dispatchDesc.MissShaderTable.SizeInBytes = uint32_t(m_MissShaders.size()) * entrySize;
            for (const auto& entry : m_MissShaders)
                writeEntry(entry);
        }

        // 写入 hit group shader
        if (!m_HitGroups.empty()) {
            dispatchDesc.HitGroupTable.StartAddress = gpuVA;
            dispatchDesc.HitGroupTable.StrideInBytes = m_HitGroups.size() == 1 ? 0 : entrySize;
            dispatchDesc.HitGroupTable.SizeInBytes = uint32_t(m_HitGroups.size()) * entrySize;
            for (const auto& entry : m_HitGroups)
                writeEntry(entry);
        }

        // 写入 callable shader
        if (!m_CallableShaders.empty()) {
            dispatchDesc.CallableShaderTable.StartAddress = gpuVA;
            dispatchDesc.CallableShaderTable.StrideInBytes = m_CallableShaders.size() == 1 ? 0 : entrySize;
            dispatchDesc.CallableShaderTable.SizeInBytes = uint32_t(m_CallableShaders.size()) * entrySize;
            for (const auto& entry : m_CallableShaders)
                writeEntry(entry);
        }

        state.committedVersion = m_Version;
        if (m_Pipeline->HasLocalResources()) {
            state.descriptorHeapSRV = resources.shaderResourceViewHeap.GetShaderVisibleHeap();
            state.descriptorHeapSampler = resources.samplerHeap.GetShaderVisibleHeap();
        }
        else {
            state.descriptorHeapSRV = nullptr;
            state.descriptorHeapSampler = nullptr;
        }
    }

    void ShaderTable::SetGenerationShader(const char *exportName, IBindingSet *bindingSet)
    {
        const auto exportEntry = m_Pipeline->GetExport(exportName);
        if(VerifyEntry(exportEntry, bindingSet)) {
            m_RayGeneration.identifier = exportEntry->identifier;
            m_RayGeneration.bindings = bindingSet;
            ++m_Version;
        }
    }

    int ShaderTable::AddMissShader(const char *exportName, IBindingSet *bindingSet)
    {
        return AddEntry(exportName, bindingSet, m_MissShaders);
    }

    int ShaderTable::AddHitGroup(const char *exportName, IBindingSet *bindingSet)
    {
        return AddEntry(exportName, bindingSet, m_HitGroups);
    }

    int ShaderTable::AddCallableShader(const char *exportName, IBindingSet *bindingSet)
    {
        return AddEntry(exportName, bindingSet, m_CallableShaders);
    }

    void ShaderTable::ClearMissShaders()
    {
        m_MissShaders.clear();
        ++m_Version;
    }

    void ShaderTable::ClearHitGroups()
    {
        m_HitGroups.clear();
        ++m_Version;
    }

    void ShaderTable::ClearCallableShaders()
    {
        m_CallableShaders.clear();
        ++m_Version;
    }

    bool ShaderTable::VerifyEntry(const RayTracingPipeline::ExportEntry *exportEntry, IBindingSet *bindingSet) const
    {
        if (!exportEntry)
        {
            m_Context.Error("Couldn't find a DXR PSO export with a given name");
            return false;
        }

        if (exportEntry->bindingLayout != nullptr && !bindingSet)
        {
            m_Context.Error("A shader table entry does not provide required local bindings");
            return false;
        }

        if (exportEntry->bindingLayout == nullptr && bindingSet != nullptr)
        {
            m_Context.Error("A shader table entry provides local bindings, but none are required");
            return false;
        }

        // 检查绑定集的根签名是否与导出条目所需的根签名匹配
        if (bindingSet != nullptr && (Utility::CheckedCast<D3D12::BindingSet*>(bindingSet)->GetLayout() != exportEntry->bindingLayout))
        {
            m_Context.Error("A shader table entry provides local bindings that do not match the expected layout");
            return false;
        }

        return true;
    }

    int ShaderTable::AddEntry(const char *exportName, IBindingSet *bindingSet, std::vector<Entry> &entryList)
    {
        const auto exportEntry = m_Pipeline->GetExport(exportName);
        
        int index = -1;
        if(VerifyEntry(exportEntry, bindingSet)) {
            entryList.push_back({ exportEntry->identifier, bindingSet });
            ++m_Version;
            index = static_cast<int>(entryList.size() - 1);
        }
        return index;
    }

} // namespace DSM::D3D12

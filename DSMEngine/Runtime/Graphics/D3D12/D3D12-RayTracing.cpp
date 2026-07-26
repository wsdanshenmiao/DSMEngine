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

        m_DataBuffer = device->CreateBuffer(BufferDesc()
            .SetCanHaveRawViews(true)
            .SetKeepInitialState(true)
            .SetIsAccelStructStorage(true)
            .SetDebugName(m_Desc.debugName)
            .SetIsVirtual(m_Desc.isVirtual)
            .SetByteSize(prebuildInfo.ResultDataMaxSizeInBytes)
            .SetInitialState(m_Desc.isTopLevel ? ResourceStates::AccelStructRead : ResourceStates::AccelStructBuildBlas));

        for(auto& geom : m_Desc.bottomLevelGeometries) {
            geom.geometryData.triangles.indexBuffer = nullptr;
            geom.geometryData.triangles.vertexBuffer = nullptr;
        }
    }

    void AccelStruct::CreateSRV(size_t descriptor) const
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = m_DataBuffer->GetGpuVirtualAddress();
    
        m_Context.device->CreateShaderResourceView(nullptr, &srvDesc, { descriptor });
    }

    Object AccelStruct::GetNativeObject(ObjectType type)
    {
        if (m_DataBuffer != nullptr)
            return m_DataBuffer->GetNativeObject(type);
        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////
    // RayTracingPipeline
    //////////////////////////////////////////////////////////////////////////
    RayTracingPipeline::RayTracingPipeline(const Context& context)
        : m_Context(context) {}

    const RayTracingPipeline::ExportEntry* RayTracingPipeline::GetExport(const char* name) const
    {
        auto it = m_Exports.find(name);
        return it == m_Exports.end() ? nullptr : &it->second;
    }

    uint32_t RayTracingPipeline::GetShaderTableEntrySize() const
    {
        uint32_t requiredSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(uint64_t) * m_MaxLocalRootSize;
        return Math::Align(requiredSize, uint32_t(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    }

    RT::ShaderTableHandle RayTracingPipeline::CreateShaderTable()
    {
        return RT::ShaderTableHandle(new ShaderTable(m_Context, this));
    }

    //////////////////////////////////////////////////////////////////////////
    // ShaderTable
    //////////////////////////////////////////////////////////////////////////
    ShaderTable::ShaderTable(const Context& context, RayTracingPipeline* pipeline)
        : m_Context(context), m_Pipeline(pipeline) {}

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

        if (exportEntry->bindingLayout && !bindingSet)
        {
            m_Context.Error("A shader table entry does not provide required local bindings");
            return false;
        }

        if (!exportEntry->bindingLayout && bindingSet)
        {
            m_Context.Error("A shader table entry provides local bindings, but none are required");
            return false;
        }

        // 检查绑定集的根签名是否与导出条目所需的根签名匹配
        if (bindingSet && (Utility::CheckedCast<D3D12::BindingSet*>(bindingSet)->GetLayout() != exportEntry->bindingLayout))
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

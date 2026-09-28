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
        if (m_Context.device5 == nullptr || device == nullptr)
            return;

        m_Desc = desc;

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs{};
        auto buildInputs = GetAccelerationStructureBuildInputs(m_Desc, geometryDescs);
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
        m_Context.device5->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

        auto buffer = device->CreateBuffer(BufferDesc()
            .SetCanHaveUAVs(true)
            .SetKeepInitialState(true)
            .SetIsAccelStructStorage(true)
            .SetDebugName(m_Desc.debugName)
            .SetIsVirtual(m_Desc.isVirtual)
            .SetByteSize(prebuildInfo.ResultDataMaxSizeInBytes)
            .SetInitialState(m_Desc.isTopLevel ? ResourceStates::AccelStructRead : ResourceStates::AccelStructBuildBlas));
        dataBuffer = Utility::CheckedCast<Buffer*>(buffer.Get());

        // 创建描述只保存预分配布局，实际资源由每次 BLAS 构建显式传入。
        for(auto& geometry : m_Desc.bottomLevelGeometries) {
            if (geometry.geometryType == RT::GeometryType::Triangles) {
                geometry.geometryData.triangles.indexBuffer = nullptr;
                geometry.geometryData.triangles.vertexBuffer = nullptr;
            }
            else if (geometry.geometryType == RT::GeometryType::AABBs) {
                geometry.geometryData.aabbs.buffer = nullptr;
            }
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
    RayTracingPipeline::RayTracingPipeline(Device* device)
        : m_Device(device) {}

    bool RayTracingPipeline::Create(RT::PipelineDesc desc)
    {
        if(m_Device == nullptr)
            return false;
        
        const auto& context = m_Device->GetContext();
        auto fallbackError = [&context](const std::string& msg = "Failed to create ray tracing pipeline") {
            context.Error(msg);
            return false;
        };

        if (context.device5 == nullptr)
            return fallbackError("Ray tracing is not supported on this device");
           
        struct Library
        {
            const void* bytecode = nullptr;
            size_t bytecodeSize = 0;
            std::vector<std::pair<std::wstring, std::wstring>> exports{};
            std::vector<D3D12_EXPORT_DESC> d3dExports{};
        };

        // 收集所有的 shader
        std::unordered_map<const void*, Library> libraries{};
        for (const auto& shaderDesc : desc.shaders) {
            if (shaderDesc.shader == nullptr) {
                return fallbackError("Ray tracing pipeline contains a null shader");
            }

            const auto shaderType = shaderDesc.shader->GetDesc().shaderType;
            if (shaderType != ShaderType::RayGeneration
                && shaderType != ShaderType::Miss
                && shaderType != ShaderType::Callable) {
                return fallbackError(std::format(
                    "Ray tracing shader export '{}' must be RayGeneration, Miss, or Callable; "
                    "hit shaders must be referenced through a hit group",
                    shaderDesc.exportName.empty()
                        ? shaderDesc.shader->GetDesc().entryName
                        : shaderDesc.exportName));
            }

            const void* bytecode = nullptr;
            size_t bytecodeSize = 0;
            shaderDesc.shader->GetBytecode(&bytecode, &bytecodeSize);
            if (bytecode == nullptr || bytecodeSize == 0) {
                return fallbackError("Ray tracing shader has empty DXIL bytecode");
            }

            auto& library = libraries[bytecode];
            library.bytecode = bytecode;
            library.bytecodeSize = bytecodeSize;

            // 若 exportName 为空，则使用 shader 的 entryName 作为导出名
            const auto& shaderName = shaderDesc.shader->GetDesc().entryName;
            const auto& exportName = shaderDesc.exportName.empty() ? shaderName : shaderDesc.exportName;
            library.exports.emplace_back(
                std::wstring(shaderName.begin(), shaderName.end()),
                std::wstring(exportName.begin(), exportName.end()));

            // 创建 shader 的局部根签名
            if (shaderDesc.bindingLayout != nullptr) {
                auto* layout = shaderDesc.bindingLayout.Get();
                if (!m_LocalRootSignatures.contains(layout)) {
                    auto rootSignature = m_Device->BuildRootSignature({ shaderDesc.bindingLayout }, false, true);
                    if (rootSignature == nullptr) {
                        return fallbackError("Failed to build local root signature for ray tracing shader");
                    }

                    m_LocalRootSignatures[layout] = Utility::CheckedCast<RootSignature*>(rootSignature.Get());
                    auto* d3dLayout = Utility::CheckedCast<BindingLayout*>(layout);
                    m_MaxLocalRootSignatureSize = std::max(
                        m_MaxLocalRootSignatureSize,
                        static_cast<uint32_t>(d3dLayout->rootParameters.size()));
                }
            }
        }

        // 收集所有的 hit group
        std::vector<D3D12_HIT_GROUP_DESC> hitGroups{};
        hitGroups.reserve(desc.hitGroups.size());
        std::unordered_map<IShader*, std::wstring> hitGroupShaderNames{};
        // 由于 D3D12_HIT_GROUP_DESC 中的 HitGroupExport 是 const wchar_t*，因此需要保存导出名的 wstring
        std::vector<std::wstring> hitGroupExportNames{};
        hitGroupExportNames.reserve(desc.hitGroups.size());
        for (const auto& hitGroupDesc : desc.hitGroups) {
            for(const auto& shader : { hitGroupDesc.closestHitShader, hitGroupDesc.anyHitShader, hitGroupDesc.intersectionShader }){
                if(shader == nullptr)
                    continue;

                if(auto name = hitGroupShaderNames.find(shader.Get()); name == hitGroupShaderNames.end()){
                    const void* bytecode = nullptr;
                    size_t bytecodeSize = 0;
                    shader->GetBytecode(&bytecode, &bytecodeSize);

                    if (bytecode == nullptr || bytecodeSize == 0) {
                        return fallbackError("Ray tracing hit group shader has empty DXIL bytecode");
                    }

                    Library& library = libraries[bytecode];
                    library.bytecode = bytecode;
                    library.bytecodeSize = bytecodeSize;
                    const auto& shaderName = shader->GetDesc().entryName;
                    std::string exportName = shaderName + std::to_string(hitGroupShaderNames.size());
                    library.exports.emplace_back(
                        std::wstring(shaderName.begin(), shaderName.end()),
                        std::wstring(exportName.begin(), exportName.end()));
                    // 记录 shader -> 导出名 的映射，后续 hit group 会使用
                    hitGroupShaderNames[shader.Get()] = std::wstring(exportName.begin(), exportName.end());
                }
            }

            if (hitGroupDesc.bindingLayout != nullptr) {
                auto* layout = hitGroupDesc.bindingLayout.Get();
                if (layout != nullptr && !m_LocalRootSignatures.contains(layout))
                {
                    auto rootSignature = m_Device->BuildRootSignature({ hitGroupDesc.bindingLayout }, false, true);
                    if (rootSignature == nullptr) {
                        return fallbackError("Failed to build local root signature for ray tracing hit group");
                    }

                    m_LocalRootSignatures[layout] = Utility::CheckedCast<RootSignature*>(rootSignature.Get());
                    auto d3dLayout = Utility::CheckedCast<BindingLayout*>(layout);
                    m_MaxLocalRootSignatureSize = std::max(
                        m_MaxLocalRootSignatureSize,
                        static_cast<uint32_t>(d3dLayout->rootParameters.size()));
                }
            }

            // 填充 hit group 结构体
            D3D12_HIT_GROUP_DESC d3dHitGroup{};
            if(hitGroupDesc.closestHitShader != nullptr){
                d3dHitGroup.ClosestHitShaderImport = hitGroupShaderNames[hitGroupDesc.closestHitShader].c_str();
            }
            if(hitGroupDesc.anyHitShader != nullptr){
                d3dHitGroup.AnyHitShaderImport = hitGroupShaderNames[hitGroupDesc.anyHitShader].c_str();
            }
            if(hitGroupDesc.intersectionShader != nullptr){
                d3dHitGroup.IntersectionShaderImport = hitGroupShaderNames[hitGroupDesc.intersectionShader].c_str();
            }
            d3dHitGroup.Type = hitGroupDesc.isProceduralPrimitive
                ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE
                : D3D12_HIT_GROUP_TYPE_TRIANGLES;
            hitGroupExportNames.emplace_back(hitGroupDesc.exportName.begin(), hitGroupDesc.exportName.end());
            d3dHitGroup.HitGroupExport = hitGroupExportNames.back().c_str();
            hitGroups.push_back(std::move(d3dHitGroup));
        }

        std::vector<D3D12_DXIL_LIBRARY_DESC> dxilLibraries;
        dxilLibraries.reserve(libraries.size());
        for (auto& [bytecode, library] : libraries) {
            for (const auto& [originalName, exportName] : library.exports) {
                D3D12_EXPORT_DESC d3dExport{};
                d3dExport.ExportToRename = originalName.c_str();
                d3dExport.Name = exportName.c_str();
                d3dExport.Flags = D3D12_EXPORT_FLAG_NONE;
                library.d3dExports.push_back(std::move(d3dExport));
            }

            D3D12_DXIL_LIBRARY_DESC d3dLibrary{};
            d3dLibrary.DXILLibrary.pShaderBytecode = library.bytecode;
            d3dLibrary.DXILLibrary.BytecodeLength = library.bytecodeSize;
            d3dLibrary.NumExports = static_cast<UINT>(library.d3dExports.size());
            d3dLibrary.pExports = library.d3dExports.empty() ? nullptr : library.d3dExports.data();
            dxilLibraries.push_back(std::move(d3dLibrary));
        }

        // 创建 state subobjects，包含管线配置、DXIL 库、hit group、全局根签名和局部根签名
        std::vector<D3D12_STATE_SUBOBJECT> stateSubobjects{};

        // 创建全局根签名
        D3D12_GLOBAL_ROOT_SIGNATURE globalRootSignature{};
        if (!desc.globalBindingLayout.empty())
        {
            auto rootSignature = m_Device->GetRootSignature(desc.globalBindingLayout, false);
            m_GlobalRootSignature = rootSignature;
            globalRootSignature.pGlobalRootSignature = rootSignature->rootSignature.Get();
        }

        stateSubobjects.reserve(2 + dxilLibraries.size() + hitGroups.size()
            + (globalRootSignature.pGlobalRootSignature != nullptr ? 1 : 0)
            + m_LocalRootSignatures.size() * 2);
        const auto addSubobject = [&stateSubobjects](D3D12_STATE_SUBOBJECT_TYPE type, const void* desc)
        {
            D3D12_STATE_SUBOBJECT subobject{};
            subobject.Type = type;
            subobject.pDesc = desc;
            stateSubobjects.push_back(subobject);
        };

        if (globalRootSignature.pGlobalRootSignature != nullptr)
            addSubobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &globalRootSignature);

        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
        shaderConfig.MaxAttributeSizeInBytes = desc.maxAttributeSize;
        shaderConfig.MaxPayloadSizeInBytes = desc.maxPayloadSize;
        addSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig);

        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{};
        pipelineConfig.MaxTraceRecursionDepth = desc.maxRecursionDepth;
        addSubobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig);

        for (const auto& library : dxilLibraries)
            addSubobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &library);
        for (const auto& hitGroup : hitGroups)
            addSubobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroup);

        std::vector<D3D12_LOCAL_ROOT_SIGNATURE> localRootSignatures{};
        std::vector<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> associations{};
        localRootSignatures.reserve(m_LocalRootSignatures.size());
        associations.reserve(m_LocalRootSignatures.size());

        std::vector<std::wstring> associationExports{};
        std::vector<LPCWSTR> associationExportPointers{};
        associationExports.reserve(desc.shaders.size() + desc.hitGroups.size());
        associationExportPointers.reserve(desc.shaders.size() + desc.hitGroups.size());

        for (const auto& [bindingLayout, rootSignature] : m_LocalRootSignatures) {
            // 添加局部根签名子对象
            auto& localRootSignature = localRootSignatures.emplace_back();
            localRootSignature.pLocalRootSignature = rootSignature->rootSignature.Get();
            addSubobject(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &localRootSignature);

            // 将局部根签名关联到对应的 shader 或 hit group 上
            auto& association = associations.emplace_back();
            association.pSubobjectToAssociate = &stateSubobjects.back();
            association.NumExports = 0;
            const size_t firstExport = associationExportPointers.size();
            for (const auto& shaderDesc : desc.shaders) {
                if (shaderDesc.bindingLayout.Get() == bindingLayout) {
                    const auto& exportName = shaderDesc.exportName.empty() ? 
                        shaderDesc.shader->GetDesc().entryName : shaderDesc.exportName;
                    associationExports.emplace_back(exportName.begin(), exportName.end());
                    associationExportPointers.push_back(associationExports.back().c_str());
                    ++association.NumExports;
                }
            }
            for (const auto& hitGroupDesc : desc.hitGroups) {
                if (hitGroupDesc.bindingLayout.Get() == bindingLayout) {
                    associationExports.emplace_back(hitGroupDesc.exportName.begin(), hitGroupDesc.exportName.end());
                    associationExportPointers.push_back(associationExports.back().c_str());
                    ++association.NumExports;
                }
            }
            association.pExports = associationExportPointers.data() + firstExport;
            addSubobject(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &association);
        }

        if (desc.hlslExtensionsUAV >= 0) {
            return fallbackError("HLSL extensions UAVs are not supported by the D3D12 backend");
        }

        D3D12_STATE_OBJECT_DESC stateObjectDesc{};
        stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        stateObjectDesc.NumSubobjects = static_cast<UINT>(stateSubobjects.size());
        stateObjectDesc.pSubobjects = stateSubobjects.data();

        auto hr = context.device5->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(m_PipelineState.GetAddressOf()));
        if (FAILED(hr)) {
            return fallbackError(std::format("Failed to create a DXR pipeline state object: {}", GetHRErrorMessage(hr)));
        }

        hr = m_PipelineState->QueryInterface(IID_PPV_ARGS(m_StateObjectInfo.GetAddressOf()));
        if (FAILED(hr)) {
            return fallbackError(std::format("Failed to query DXR pipeline state properties: {}", GetHRErrorMessage(hr)));
        }

        for (const auto& shaderDesc : desc.shaders) {
            const auto& shaderName = shaderDesc.shader->GetDesc().entryName;
            const auto& exportName = shaderDesc.exportName.empty() ? shaderName : shaderDesc.exportName;
            std::wstring exportNameWide(exportName.begin(), exportName.end());
            const void* identifier = m_StateObjectInfo->GetShaderIdentifier(exportNameWide.c_str());
            if (identifier == nullptr) {
                return fallbackError(std::format("Failed to get the DXR shader identifier for export '{}'", exportName));
            }

            m_Exports[exportName] = RayTracingPipeline::ExportEntry{shaderDesc.bindingLayout.Get(), identifier };
        }

        for (const auto& hitGroupDesc : desc.hitGroups) {
            std::wstring exportNameWide(hitGroupDesc.exportName.begin(), hitGroupDesc.exportName.end());
            const void* identifier = m_StateObjectInfo->GetShaderIdentifier(exportNameWide.c_str());
            if (identifier == nullptr) {
                return fallbackError(std::format("Failed to get the DXR hit-group identifier for export '{}'", hitGroupDesc.exportName));
            }

            m_Exports[hitGroupDesc.exportName] = RayTracingPipeline::ExportEntry{hitGroupDesc.bindingLayout.Get(), identifier };
        }
        
        m_Desc = desc;
        return true;
    }

    const RayTracingPipeline::ExportEntry *RayTracingPipeline::GetExport(const char *name) const
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
        if(m_Device == nullptr)
            return nullptr;

        const auto& context = m_Device->GetContext();
        BufferHandle cache{};
        if (desc.isCached)
        {
            if (desc.maxEntries == 0)
            {
                context.Error("maxEntries must be nonzero for a cached ShaderTable");
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

        auto shaderTable = new ShaderTable(context, this, desc);
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

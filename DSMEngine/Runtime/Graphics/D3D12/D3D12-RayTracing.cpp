#include "D3D12-RayTracing.h"
#include "D3D12-Device.h"
#include "D3D12-CommandList.h"
#include <cstring>
#include <format>

namespace DSM::D3D12 {

    //////////////////////////////////////////////////////////////////////////
    // 标志位转换辅助
    //////////////////////////////////////////////////////////////////////////
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ConvertAccelStructBuildFlags(RT::AccelStructBuildFlags flags)
    {
        using T = std::underlying_type_t<RT::AccelStructBuildFlags>;
        T f = (T)flags;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS result = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        if (f & (T)RT::AccelStructBuildFlags::AllowUpdate)     result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
        if (f & (T)RT::AccelStructBuildFlags::AllowCompaction) result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
        if (f & (T)RT::AccelStructBuildFlags::PreferFastTrace) result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        if (f & (T)RT::AccelStructBuildFlags::PreferFastBuild) result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
        if (f & (T)RT::AccelStructBuildFlags::MinimizeMemory)  result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;
        if (f & (T)RT::AccelStructBuildFlags::PerformUpdate)   result |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        return result;
    }

    D3D12_RAYTRACING_INSTANCE_FLAGS ConvertInstanceFlags(RT::InstanceFlags flags)
    {
        using T = std::underlying_type_t<RT::InstanceFlags>;
        T f = (T)flags;
        D3D12_RAYTRACING_INSTANCE_FLAGS result = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        if (f & (T)RT::InstanceFlags::ForceOpaque)    result |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
        if (f & (T)RT::InstanceFlags::ForceNonOpaque) result |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
        if (f & (T)RT::InstanceFlags::TriangleCullDisable) result |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
        if (f & (T)RT::InstanceFlags::TriangleFrontCounterClockwise) result |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
        return result;
    }

    //////////////////////////////////////////////////////////////////////////
    // AccelStruct
    //////////////////////////////////////////////////////////////////////////
    AccelStruct::AccelStruct(const Context& context)
        : m_Context(context)
    {
    }

    bool AccelStruct::Finalize(const RT::AccelStructDesc& desc, Device* device)
    {
        m_Desc = desc;

        // 构建输入描述以查询预构建信息（顶点/索引的 GPU 地址在构建阶段才需要）
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Type = desc.isTopLevel
            ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL
            : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        inputs.Flags = ConvertAccelStructBuildFlags(desc.buildFlags);

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries;
        if (!desc.isTopLevel) {
            geometries.resize(desc.geometries.size());
            for (size_t i = 0; i < desc.geometries.size(); ++i) {
                const auto& g = desc.geometries[i];
                auto& geo = geometries[i];
                geo.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                geo.Flags = g.isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
                geo.Triangles.VertexFormat = GetDxgiFormatMapping(g.vertexFormat).srvFormat;
                geo.Triangles.VertexCount = g.vertexCount;
                geo.Triangles.VertexBuffer.StrideInBytes = g.vertexStride;
                geo.Triangles.VertexBuffer.StartAddress = 0;
                if (g.isIndexed) {
                    geo.Triangles.IndexFormat = GetDxgiFormatMapping(g.indexFormat).srvFormat;
                    geo.Triangles.IndexCount = g.indexCount;
                    geo.Triangles.IndexBuffer = 0;
                }
                else {
                    geo.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
                    geo.Triangles.IndexCount = 0;
                    geo.Triangles.IndexBuffer = 0;
                }
                geo.Triangles.Transform3x4 = 0;
            }
            inputs.NumDescs = UINT(geometries.size());
            inputs.pGeometryDescs = geometries.data();
        }
        else {
            inputs.NumDescs = desc.instanceCount;
            inputs.InstanceDescs = 0; // 构建阶段填充
        }

        m_Context.device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &m_PrebuildInfo);

        // 创建虚拟 dataBuffer（结果数据），稍后由 BindAccelStructMemory 放置到堆
        BufferDesc bufferDesc{};
        bufferDesc.byteSize = m_PrebuildInfo.ResultDataMaxSizeInBytes;
        bufferDesc.isAccelStructStorage = true;
        bufferDesc.isVirtual = true;
        bufferDesc.canHaveUAVs = true;
        bufferDesc.keepInitialState = true;
        bufferDesc.initialState = desc.isTopLevel ? ResourceStates::AccelStructRead : ResourceStates::AccelStructBuildBlas;
        bufferDesc.debugName = desc.debugName.empty() ? "AccelStruct" : desc.debugName;
        m_DataBuffer = Utility::CheckedCast<Buffer*>(device->CreateBuffer(bufferDesc).Get());
        if (m_DataBuffer == nullptr) {
            m_Context.Error("Failed to create acceleration structure data buffer.");
            return false;
        }
        return true;
    }

    uint64_t AccelStruct::GetDeviceAddress() const
    {
        return m_DataBuffer != nullptr ? m_DataBuffer->GetGpuVirtualAddress() : 0;
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
    RayTracingPipeline::RayTracingPipeline(const Context& context, Device* device)
        : m_Context(context), m_Device(device)
    {
    }

    bool RayTracingPipeline::Finalize(const RT::PipelineDesc& desc)
    {
        m_Desc = desc;
        m_ShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

        // 1) 收集去重的着色器库与导出名
        std::unordered_map<IShaderLibrary*, std::vector<std::string>> libExportNames;
        for (const auto& s : desc.shaders) {
            if (s.shader == nullptr || s.exportName.empty()) continue;
            libExportNames[s.shader].push_back(s.exportName);
        }

        // 2) 组装状态对象子对象（内部描述需保持存活至 CreateStateObject）。
        // 子对象保存的是这些 vector 元素的地址，必须预留容量，避免 push_back 重分配导致悬垂指针。
        std::vector<D3D12_STATE_SUBOBJECT> subobjects;
        std::vector<D3D12_DXIL_LIBRARY_DESC> dxilLibDescs;
        std::vector<std::vector<D3D12_EXPORT_DESC>> exportDescArrays;
        std::vector<std::wstring> wnameStorage; // 保证 LPCWSTR 生命周期
        std::vector<D3D12_HIT_GROUP_DESC> hitGroupDescs;
        subobjects.reserve(libExportNames.size() + desc.hitGroups.size() + 3);
        dxilLibDescs.reserve(libExportNames.size());
        exportDescArrays.reserve(libExportNames.size());
        hitGroupDescs.reserve(desc.hitGroups.size());
        wnameStorage.reserve(desc.shaders.size() + desc.hitGroups.size() * 4);
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{};
        D3D12_GLOBAL_ROOT_SIGNATURE globalRS{};

        // 库子对象
        for (auto& [lib, names] : libExportNames) {
            const void* pBytecode = nullptr;
            size_t bytecodeSize = 0;
            lib->GetBytecode(&pBytecode, &bytecodeSize);

            exportDescArrays.emplace_back();
            auto& exports = exportDescArrays.back();
            for (const auto& name : names) {
                wnameStorage.push_back(Utility::UTF8ToWString(name));
                D3D12_EXPORT_DESC ed{};
                ed.Name = wnameStorage.back().c_str();
                ed.ExportToRename = nullptr;
                ed.Flags = D3D12_EXPORT_FLAG_NONE;
                exports.push_back(ed);
            }

            D3D12_DXIL_LIBRARY_DESC libDesc{};
            libDesc.DXILLibrary = { pBytecode, bytecodeSize };
            libDesc.NumExports = UINT(exports.size());
            libDesc.pExports = exports.data();
            dxilLibDescs.push_back(libDesc);

            subobjects.push_back({ D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxilLibDescs.back() });
        }

        // 命中组子对象
        for (const auto& hg : desc.hitGroups) {
            if (hg.hitGroupName.empty()) continue;
            wnameStorage.push_back(Utility::UTF8ToWString(hg.hitGroupName));
            D3D12_HIT_GROUP_DESC hgd{};
            hgd.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
            hgd.HitGroupExport = wnameStorage.back().c_str();
            if (!hg.closestHitShader.empty()) {
                wnameStorage.push_back(Utility::UTF8ToWString(hg.closestHitShader));
                hgd.ClosestHitShaderImport = wnameStorage.back().c_str();
            }
            if (!hg.anyHitShader.empty()) {
                wnameStorage.push_back(Utility::UTF8ToWString(hg.anyHitShader));
                hgd.AnyHitShaderImport = wnameStorage.back().c_str();
            }
            if (!hg.intersectionShader.empty()) {
                wnameStorage.push_back(Utility::UTF8ToWString(hg.intersectionShader));
                hgd.IntersectionShaderImport = wnameStorage.back().c_str();
            }
            hitGroupDescs.push_back(hgd);
            subobjects.push_back({ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDescs.back() });
        }

        // 着色器配置（payload / attribute 大小）
        shaderConfig.MaxPayloadSizeInBytes = desc.maxPayloadSize;
        shaderConfig.MaxAttributeSizeInBytes = desc.maxAttributeSize;
        subobjects.push_back({ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig });

        // 管线配置（最大递归深度）
        pipelineConfig.MaxTraceRecursionDepth = desc.maxRecursionDepth;
        subobjects.push_back({ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig });

        // 全局根签名
        if (!desc.globalBindingLayouts.empty()) {
            BindingLayoutVector layouts;
            for (auto* l : desc.globalBindingLayouts) {
                if (l != nullptr) layouts.push_back(RefPtr<IBindingLayout>(l));
            }
            m_GlobalRootSignature = Utility::CheckedCast<RootSignature*>(m_Device->BuildRootSignature(layouts, false, false).Get());
            if (m_GlobalRootSignature != nullptr) {
                globalRS.pGlobalRootSignature = m_GlobalRootSignature->rootSignature;
                subobjects.push_back({ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &globalRS });
            }
        }

        // 创建状态对象
        D3D12_STATE_OBJECT_DESC stateObjectDesc{};
        stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        stateObjectDesc.NumSubobjects = UINT(subobjects.size());
        stateObjectDesc.pSubobjects = subobjects.data();

        HRESULT hr = m_Context.device5->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(m_StateObject.GetAddressOf()));
        if (FAILED(hr)) {
            m_Context.Error(std::format("Failed to create ray tracing state object. Error msg: {}", GetHRErrorMessage(hr)));
            return false;
        }
        hr = m_StateObject->QueryInterface(IID_PPV_ARGS(m_StateObjectInfo.GetAddressOf()));
        if (FAILED(hr)) {
            m_Context.Error(std::format("Failed to query ID3D12StateObjectProperties. Error msg: {}", GetHRErrorMessage(hr)));
            return false;
        }

        // 解析导出标识符
        auto resolve = [&](const std::string& name) {
            if (name.empty()) return;
            ExportEntry entry{};
            entry.identifier = m_StateObjectInfo->GetShaderIdentifier(Utility::UTF8ToWString(name).c_str());
            m_Exports[name] = entry;
        };
        for (const auto& s : desc.shaders)
            resolve(s.exportName);
        for (const auto& hg : desc.hitGroups)
            resolve(hg.hitGroupName);

        return true;
    }

    const RayTracingPipeline::ExportEntry* RayTracingPipeline::GetExport(const char* name) const
    {
        auto it = m_Exports.find(name);
        return it != m_Exports.end() ? &it->second : nullptr;
    }

    Object RayTracingPipeline::GetNativeObject(ObjectType type)
    {
        if (type == ObjectTypes::D3D12_PipelineState)
            return m_StateObject.Get();
        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////
    // ShaderTable
    //////////////////////////////////////////////////////////////////////////
    ShaderTable::ShaderTable(RayTracingPipeline* pipeline, const RT::ShaderTableDesc& desc)
        : m_Pipeline(pipeline), m_Desc(desc)
    {
        auto resolve = [&](const std::string& name, IBindingSet* binding) -> Entry {
            Entry e{};
            if (!name.empty()) {
                const auto* ex = pipeline->GetExport(name.c_str());
                e.identifier = (ex != nullptr) ? ex->identifier : nullptr;
            }
            e.bindings = binding;
            return e;
        };

        m_RayGeneration = resolve(desc.rayGenerationShader,
            desc.rayGenerationBindings.empty() ? nullptr : desc.rayGenerationBindings[0]);
        for (size_t i = 0; i < desc.missShaders.size(); ++i)
            m_MissShaders.push_back(resolve(desc.missShaders[i],
                i < desc.missBindings.size() ? desc.missBindings[i] : nullptr));
        for (size_t i = 0; i < desc.hitGroups.size(); ++i)
            m_HitGroups.push_back(resolve(desc.hitGroups[i],
                i < desc.hitGroupBindings.size() ? desc.hitGroupBindings[i] : nullptr));
        for (size_t i = 0; i < desc.callableShaders.size(); ++i)
            m_CallableShaders.push_back(resolve(desc.callableShaders[i],
                i < desc.callableBindings.size() ? desc.callableBindings[i] : nullptr));

        m_NumEntries = 1u + (uint32_t)m_MissShaders.size() + (uint32_t)m_HitGroups.size() + (uint32_t)m_CallableShaders.size();
    }

    void ShaderTable::Bake(CommandList& cmdList, D3D12_DISPATCH_RAYS_DESC& outDesc) const
    {
        // v1：不含本地根参数数据，记录步长固定为 32 字节（D3D12 要求对齐）
        const uint32_t stride = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        const uint64_t totalSize = uint64_t(stride) * m_NumEntries;

        // 着色器表需 CPU 写入，使用可映射的上传分配器（gpuBufferAllocator 为 GpuExclusive，不可映射）
        DynamicResourceLocation loc = cmdList.AllocateUploadBuffer(totalSize);
        uint8_t* cpu = reinterpret_cast<uint8_t*>(loc.mappedAddress);
        std::memset(cpu, 0, totalSize);

        auto writeRecord = [&](uint32_t index, const Entry& e) {
            if (e.identifier != nullptr) {
                std::memcpy(cpu + index * stride, e.identifier, m_Pipeline->GetShaderIdentifierSize());
            }
        };

        uint32_t idx = 0;
        writeRecord(idx++, m_RayGeneration);
        for (const auto& e : m_MissShaders) writeRecord(idx++, e);
        for (const auto& e : m_HitGroups) writeRecord(idx++, e);
        for (const auto& e : m_CallableShaders) writeRecord(idx++, e);

        outDesc.RayGenerationShaderRecord = { loc.gpuAddress, stride };
        if (!m_MissShaders.empty()) {
            outDesc.MissShaderTable = { loc.gpuAddress + stride,
                m_MissShaders.size() == 1 ? 0u : stride, stride * (uint32_t)m_MissShaders.size() };
        }
        else {
            outDesc.MissShaderTable = { 0, 0, 0 };
        }
        const uint64_t hitOffset = uint64_t(stride) * (1 + m_MissShaders.size());
        if (!m_HitGroups.empty()) {
            outDesc.HitGroupTable = { loc.gpuAddress + hitOffset,
                m_HitGroups.size() == 1 ? 0u : stride, stride * (uint32_t)m_HitGroups.size() };
        }
        else {
            outDesc.HitGroupTable = { 0, 0, 0 };
        }
        const uint64_t callableOffset = hitOffset + uint64_t(stride) * m_HitGroups.size();
        if (!m_CallableShaders.empty()) {
            outDesc.CallableShaderTable = { loc.gpuAddress + callableOffset,
                m_CallableShaders.size() == 1 ? 0u : stride, stride * (uint32_t)m_CallableShaders.size() };
        }
        else {
            outDesc.CallableShaderTable = { 0, 0, 0 };
        }
    }

} // namespace DSM::D3D12

#include "D3D12-Buffer.h"
#include "D3D12-Device.h"
#include <format>

namespace DSM::D3D12{
    bool Buffer::Create(BufferDesc desc)
    {
        auto createSuccess = [this]() {
            m_Context.stateTracker->RegisterBuffer(this);
            return true;
        };

        // 常量缓冲区需要对齐
        if(desc.isConstantBuffer)
            desc.byteSize = Math::Align(desc.byteSize, uint64_t(c_ConstantBufferOffsetSizeAlignment));

        m_Desc = desc;
    
        if(desc.isVolatile) 
            return createSuccess();

        resourceDesc.Width = desc.byteSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc = {1, 0};
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        
        if(desc.canHaveUAVs) 
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        if(desc.isVirtual)
            return createSuccess();

        D3D12_HEAP_PROPERTIES heapProp{};
        D3D12_HEAP_FLAGS heapFlags{};
        D3D12_RESOURCE_STATES resourceState{};

        bool isShared = false;
        if(HasFlags(desc.sharedResourceFlags, SharedResourceFlags::Shared)){
            heapFlags |= D3D12_HEAP_FLAG_SHARED;
            isShared = true;
        }
        if(HasFlags(desc.sharedResourceFlags, SharedResourceFlags::Shared_CrossAdapter)){
            heapFlags |= D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
            isShared = true;
        }

        switch (desc.cpuAccess) {
        case CpuAccessMode::None:{
            heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
            resourceState = ConvertResourceStates(desc.initialState);
            if(resourceState != D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
                resourceState = D3D12_RESOURCE_STATE_COMMON;
            break;
        }
        case CpuAccessMode::Read:{
            heapProp.Type = D3D12_HEAP_TYPE_READBACK;
            resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
            break;
        }
        case CpuAccessMode::Write:{
            heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
            resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        }
        }
        
        // Allow readback buffers to be used as resolve destination targets
        if ((desc.cpuAccess == CpuAccessMode::Read) && (desc.initialState == ResourceStates::ResolveDest)) {
            heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
            heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
            heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
            resourceState = D3D12_RESOURCE_STATE_COMMON;
        }

        auto hr = m_Context.device->CreateCommittedResource(
            &heapProp, heapFlags, 
            &resourceDesc, resourceState,
            nullptr, IID_PPV_ARGS(resource.GetAddressOf()));

        if (FAILED(hr))
        {
            std::string msg = std::format("CreateCommittedResource call failed for buffer {}, error msg: {}.",
                DebugNameToString(desc.debugName), GetHRErrorMessage(hr));
            m_Context.Error(msg);
            return false;
        }
        m_GpuVA = resource->GetGPUVirtualAddress();
        if(!desc.debugName.empty()){
            auto name = Utility::UTF8ToWString(desc.debugName);
            resource->SetName(name.c_str());
        }

        return createSuccess();
    }

    void Buffer::Create(BufferDesc desc, ID3D12Resource *resource)
    {
        assert(resource != nullptr);
        
        resource = resource;
        m_GpuVA = resource->GetGPUVirtualAddress();
        if(!desc.debugName.empty()){
            auto name = Utility::UTF8ToWString(desc.debugName);
            resource->SetName(name.c_str());
        }
        m_Desc = std::move(desc);

        m_Context.stateTracker->RegisterBuffer(this);
    }

    void Buffer::Destroy()
    {
        m_Context.stateTracker->UnregisterBuffer(this);
        if(m_Context.logBufferLifetime && resource != nullptr){
            m_Context.Info(std::format("Release buffer: {} {:#x}", 
                m_Desc.debugName, resource->GetGPUVirtualAddress()));
        }
        if(m_ClearUAV != c_InvalidDescriptorIndex){
            if(auto resource = m_Resources.lock()){
                resource->shaderResourceViewHeap.ReleaseDescriptor(m_ClearUAV);
            }
            m_ClearUAV = c_InvalidDescriptorIndex;
        }

        resource = nullptr;
        heap = nullptr;
    }

    Object Buffer::GetNativeObject(ObjectType type)
    {
        switch (type)
        {
        case ObjectTypes::D3D12_Resource:
            return Object{resource};
        case ObjectTypes::SharedHandle:
            return Object{sharedHandle};
        default:
            return Object{nullptr};
        }
    }
    
    void Buffer::CreateCBV(size_t descriptor, BufferRange range) const
    {
        assert(m_Desc.isConstantBuffer);

        range = range.Resolve(m_Desc);
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = m_GpuVA + range.byteOffset;
        cbvDesc.SizeInBytes = Math::Align((UINT)range.byteSize, c_ConstantBufferOffsetSizeAlignment);
        m_Context.device->CreateConstantBufferView(&cbvDesc, {descriptor});
    }
    
    void Buffer::CreateSRV(size_t descriptor, Format format, BufferRange range, ResourceType type) const
    {
        range = range.Resolve(m_Desc);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

        format = format == Format::UNKNOWN ? m_Desc.format : format;

        switch (type)
        {
        case ResourceType::StructuredBuffer_SRV:{
            assert(m_Desc.structStride != 0);
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.Buffer.FirstElement = range.byteOffset / m_Desc.structStride;
            srvDesc.Buffer.NumElements = range.byteSize / m_Desc.structStride;
            srvDesc.Buffer.StructureByteStride = m_Desc.structStride;
            break;
        }
        case ResourceType::RawBuffer_SRV:{
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.Buffer.FirstElement = range.byteOffset / 4;
            srvDesc.Buffer.NumElements = range.byteSize / 4;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            break;
        }
        case ResourceType::TypedBuffer_SRV:{
            assert(format != Format::UNKNOWN);
            const auto& formatMapping = GetDxgiFormatMapping(format);
            const FormatInfo& formatInfo = GetFormatInfo(format);
            srvDesc.Format = formatMapping.srvFormat;
            srvDesc.Buffer.FirstElement = range.byteOffset / formatInfo.bytesPerBlock;
            srvDesc.Buffer.NumElements = range.byteSize / formatInfo.bytesPerBlock;
            break;
        }
        default:
            m_Context.Error("Invalid resource type.");
            return;
        }

        m_Context.device->CreateShaderResourceView(resource.Get(), &srvDesc, {descriptor});
    }
    
    void Buffer::CreateUAV(size_t descriptor, Format format, BufferRange range, ResourceType type) const
    {
        range = range.Resolve(m_Desc);
        
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        
        format = format == Format::UNKNOWN ? m_Desc.format : format;

        switch (type)
        {
        case ResourceType::StructuredBuffer_UAV:{
            assert(m_Desc.structStride != 0);
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.Buffer.FirstElement = range.byteOffset / m_Desc.structStride;
            uavDesc.Buffer.NumElements = (UINT)(range.byteSize / m_Desc.structStride);
            uavDesc.Buffer.StructureByteStride = m_Desc.structStride;
            break;
        }
        case ResourceType::RawBuffer_UAV:{
            uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            uavDesc.Buffer.FirstElement = range.byteOffset / 4;
            uavDesc.Buffer.NumElements = (UINT)(range.byteSize / 4);
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;
        }
        case ResourceType::TypedBuffer_UAV: {
            assert(format != Format::UNKNOWN);
            const DxgiFormatMapping& mapping = GetDxgiFormatMapping(format);
            const FormatInfo& formatInfo = GetFormatInfo(format);
            uavDesc.Format = mapping.srvFormat;
            uavDesc.Buffer.FirstElement = range.byteOffset / formatInfo.bytesPerBlock;
            uavDesc.Buffer.NumElements = (UINT)(range.byteSize / formatInfo.bytesPerBlock);
            break;
        }
        default: 
            m_Context.Error("Invalid resource type.");
            return;
        }

        m_Context.device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, { descriptor });
    }

    uint32_t Buffer::GetClearUAV()
    {
        assert(m_Desc.canHaveUAVs);
        if(m_ClearUAV != c_InvalidDescriptorIndex) return m_ClearUAV;

        if(auto resources = m_Resources.lock()){
            m_ClearUAV = resources->shaderResourceViewHeap.AllocateDescriptor();
            auto cpuHandle = resources->shaderResourceViewHeap.GetCpuHandle(m_ClearUAV);
            CreateUAV(cpuHandle.ptr, Format::R32_UINT, EntireBuffer, ResourceType::TypedBuffer_UAV);
            resources->shaderResourceViewHeap.CopyToShaderVisibleHeap(m_ClearUAV);
        }
        return m_ClearUAV;
    }

    void Buffer::CreateNullSRV(size_t descriptor, Format format, const Context &context)
    {
        const auto& mapping = GetDxgiFormatMapping(format == Format::UNKNOWN ? Format::R32_UINT : format);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = mapping.srvFormat;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        context.device->CreateShaderResourceView(nullptr, &srvDesc, { descriptor });
    }
    
    void Buffer::CreateNullUAV(size_t descriptor, Format format, const Context &context)
    {
        const auto& mapping = GetDxgiFormatMapping(format == Format::UNKNOWN ? Format::R32_UINT : format);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = mapping.srvFormat;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        context.device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, { descriptor });
    }
}

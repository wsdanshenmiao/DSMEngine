#include "D3D12-Buffer.h"
#include <format>

namespace DSM::D3D12{
    Buffer::~Buffer()
    {
        if(m_Context.m_LogBufferLifetime){
            m_Context.Info(std::format("Release buffer: {} {:#x}", 
                m_Desc.debugName, resource->GetGPUVirtualAddress()));
        }
        if(m_ClearUAV != c_InvalidDescriptorIndex){
            m_Resources.shaderResourceViewHeap.ReleaseDescriptor(m_ClearUAV);
            m_ClearUAV = c_InvalidDescriptorIndex;
        }
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
        cbvDesc.SizeInBytes = Utility::Align((UINT)range.byteSize, c_ConstantBufferOffsetSizeAlignment);
        m_Context.m_Device->CreateConstantBufferView(&cbvDesc, {descriptor});
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

        m_Context.m_Device->CreateShaderResourceView(resource.Get(), &srvDesc, {descriptor});
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

        m_Context.m_Device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, { descriptor });
    }

    uint32_t Buffer::GetClearUAV()
    {
        assert(m_Desc.canHaveUAVs);
        if(m_ClearUAV != c_InvalidDescriptorIndex) return m_ClearUAV;

        m_ClearUAV = m_Resources.shaderResourceViewHeap.AllocateDescriptor();
        auto cpuHandle = m_Resources.shaderResourceViewHeap.GetCpuHandle(m_ClearUAV);
        CreateUAV(cpuHandle.ptr, Format::R32_UINT, EntireBuffer, ResourceType::TypedBuffer_UAV);
        m_Resources.shaderResourceViewHeap.CopyToShaderVisibleHeap(m_ClearUAV);
        return m_ClearUAV;
    }

    void Buffer::CreateNullSRV(size_t descriptor, Format format, const Context &context)
    {
        const auto& mapping = GetDxgiFormatMapping(format == Format::UNKNOWN ? Format::R32_UINT : format);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = mapping.srvFormat;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        context.m_Device->CreateShaderResourceView(nullptr, &srvDesc, { descriptor });
    }
    
    void Buffer::CreateNullUAV(size_t descriptor, Format format, const Context &context)
    {
        const auto& mapping = GetDxgiFormatMapping(format == Format::UNKNOWN ? Format::R32_UINT : format);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = mapping.srvFormat;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        context.m_Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, { descriptor });
    }
}

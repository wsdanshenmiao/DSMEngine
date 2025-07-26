#include "D3D12-Texture.h"


namespace DSM::D3D12 {
    Texture::~Texture()
    {
        for(const auto& [bindingKey, index] : m_RenderTargetViews){
            m_Resources.renderTargetViewHeap.ReleaseDescriptor(index);
        }
        for(const auto& [bindingKey, index] : m_DepthStencilViews){
            m_Resources.depthStencilViewHeap.ReleaseDescriptor(index);
        }
        for(const auto& [bindingKey, index] : m_CustomSRVs){
            m_Resources.shaderResourceViewHeap.ReleaseDescriptor(index);
        }
        for(const auto& [bindingKey, index] : m_CustomUAVs){
            m_Resources.shaderResourceViewHeap.ReleaseDescriptor(index);
        }
    }

    Object Texture::GetNativeObject(ObjectType type)
    {
        switch (type)
        {
        case ObjectTypes::D3D12_Resource:
            return Object{resource};
        case ObjectTypes::SharedHandle:
            return Object{sharedHandle};
        default:
            return nullptr;
        }
    }

    Object Texture::GetNativeView(
        ObjectType objType, 
        Format format, 
        TextureSubresourceSet subresources, 
        TextureDimension dimension, 
        bool isReadOnlyDSV)
    {
    }
    
    void Texture::CreateSRV(size_t descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const
    {
        subresources = subresources.Resolve(m_Desc, false);

        dimension = dimension == TextureDimension::Unknown ? m_Desc.dimension : dimension;
        format = format == Format::UNKNOWN ? m_Desc.format : format;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = GetDxgiFormatMapping(format).srvFormat;

        uint32_t planeSlice = (srvDesc.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT) ? 1 : 0;

        switch (dimension)
        {
        case TextureDimension::Texture1D:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            srvDesc.Texture1D.MostDetailedMip = subresources.baseMipLevel;
            srvDesc.Texture1D.MipLevels = subresources.numMipLevels;
            break;
        }
        case TextureDimension::Texture1DArray:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            srvDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
            srvDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
            srvDesc.Texture1DArray.MostDetailedMip = subresources.baseMipLevel;
            srvDesc.Texture1DArray.MipLevels = subresources.numMipLevels;
            break;
        }
        case TextureDimension::Texture2D:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = subresources.baseMipLevel;
            srvDesc.Texture2D.MipLevels = subresources.numMipLevels;
            srvDesc.Texture2D.PlaneSlice = planeSlice;
            break;
        }
        case TextureDimension::Texture2DArray:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
            srvDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
            srvDesc.Texture2DArray.MostDetailedMip = subresources.baseMipLevel;
            srvDesc.Texture2DArray.MipLevels = subresources.numMipLevels;
            srvDesc.Texture2DArray.PlaneSlice = planeSlice;
            break;
        }
        case TextureDimension::TextureCube:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = subresources.baseMipLevel;
            srvDesc.TextureCube.MipLevels = subresources.numMipLevels;
            break;
        }
        case TextureDimension::TextureCubeArray:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            srvDesc.TextureCubeArray.First2DArrayFace = subresources.baseArraySlice;
            srvDesc.TextureCubeArray.NumCubes = subresources.numArraySlices / 6;
            srvDesc.TextureCubeArray.MostDetailedMip = subresources.baseMipLevel;
            srvDesc.TextureCubeArray.MipLevels = subresources.numMipLevels;
            break;
        }
        case TextureDimension::Texture2DMS:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
            break;
        }
        case TextureDimension::Texture2DMSArray:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
            srvDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
            srvDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
            break;
        }
        case TextureDimension::Texture3D:{
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MostDetailedMip = subresources.baseMipLevel;
            srvDesc.Texture3D.MipLevels = subresources.numMipLevels;
            break;
        }
        case TextureDimension::Unknown:
        default:
            m_Context.Error("Invalid texture dimension.");
            return;
        }

        m_Context.m_Device->CreateShaderResourceView(resource.Get(), &srvDesc, {descriptor});
    }
    
    void Texture::CreateUAV(size_t descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const
    {
        subresources = subresources.Resolve(m_Desc, true);

        format = format == Format::UNKNOWN ? m_Desc.format : format;
        dimension = dimension == TextureDimension::Unknown ? m_Desc.dimension : dimension;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = GetDxgiFormatMapping(format).srvFormat;
        
        switch (dimension)
        {
        case TextureDimension::Texture1D:{
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            uavDesc.Texture1D.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture1DArray:{
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            uavDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
            uavDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
            uavDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2D:{
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:{
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
            uavDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
            uavDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2DMS:{
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMS;
            break;
        }
        case TextureDimension::Texture2DMSArray:{
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY;
            uavDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
            uavDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
            break;
        }
        case TextureDimension::Texture3D:{
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            uavDesc.Texture3D.FirstWSlice = 0;
            uavDesc.Texture3D.WSize = m_Desc.depth;
            uavDesc.Texture3D.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Unknown:
        default:
            m_Context.Error("Invalid texture dimension.");
            break;
        }

        m_Context.m_Device->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, {descriptor});
    }

    void Texture::CreateRTV(size_t descriptor, Format format, TextureSubresourceSet subresources) const
    {        
        subresources = subresources.Resolve(m_Desc, true);

        format = format == Format::UNKNOWN ? m_Desc.format : format;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = GetDxgiFormatMapping(format).rtvFormat;

        switch (m_Desc.dimension)
        {
        case TextureDimension::Texture1D:{
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            rtvDesc.Texture1D.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture1DArray:{
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            rtvDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
            rtvDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
            rtvDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2D:{
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:{
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
            rtvDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
            rtvDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2DMS:{
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            break;
        }
        case TextureDimension::Texture2DMSArray:{
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
            rtvDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
            rtvDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
            break;
        }
        case TextureDimension::Texture3D:{
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
            rtvDesc.Texture3D.FirstWSlice = 0;
            rtvDesc.Texture3D.WSize = m_Desc.depth;
            rtvDesc.Texture3D.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Unknown:
        default:
            m_Context.Error("Invalid texture dimension.");
            break;
        }

        m_Context.m_Device->CreateRenderTargetView(resource.Get(), &rtvDesc, {descriptor});
    }

    void Texture::CreateUAV(size_t descriptor, TextureSubresourceSet subresources, bool isReadOnly) const
    {    
        subresources = subresources.Resolve(m_Desc, true);

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = GetDxgiFormatMapping(m_Desc.format).rtvFormat;

        if (isReadOnly) {
            dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
            if (dsvDesc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || dsvDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
                dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        }
        
        switch (m_Desc.dimension)
        {
        case TextureDimension::Texture1D:{
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            dsvDesc.Texture1D.MipSlice = subresources.baseMipLevel;
        }
            break;
        case TextureDimension::Texture1DArray:{
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            dsvDesc.Texture1DArray.FirstArraySlice = subresources.baseArraySlice;
            dsvDesc.Texture1DArray.ArraySize = subresources.numArraySlices;
            dsvDesc.Texture1DArray.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2D:{
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:{
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.ArraySize = subresources.numArraySlices;
            dsvDesc.Texture2DArray.FirstArraySlice = subresources.baseArraySlice;
            dsvDesc.Texture2DArray.MipSlice = subresources.baseMipLevel;
            break;
        }
        case TextureDimension::Texture2DMS:{
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            break;
        }
        case TextureDimension::Texture2DMSArray:{
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
            dsvDesc.Texture2DMSArray.FirstArraySlice = subresources.baseArraySlice;
            dsvDesc.Texture2DMSArray.ArraySize = subresources.numArraySlices;
            break;
        }
        case TextureDimension::Texture3D:
        case TextureDimension::Unknown: 
        default:
            m_Context.Error("Invalid texture dimension.");
            return;
        }

        m_Context.m_Device->CreateDepthStencilView(resource.Get(), &dsvDesc, {descriptor});
    }

} // namespace DSM::D3D12

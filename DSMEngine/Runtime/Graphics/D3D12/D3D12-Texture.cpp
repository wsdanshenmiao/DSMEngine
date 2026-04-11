#include "D3D12-Texture.h"
#include "D3D12-Device.h"

namespace DSM::D3D12 {
    Texture::Texture(const Context &context, std::shared_ptr<DeviceResources> resources)
        :m_Context(context), m_Resources(resources) {}

    bool Texture::Create(TextureDesc desc)
    {
        auto createSuccess = [this]() {
            m_Context.stateTracker->RegisterTexture(this);
            return true;
        };

        m_Desc = desc;
        resourceDesc = ConvertTextureDesc(desc);

        if(desc.isUAV){
            m_ClearMipLevelUAVs.resize(desc.mipLevels, c_InvalidDescriptorIndex);
        }

        auto resources = m_Resources.lock();
        if(resources == nullptr){
            m_Context.Error("Device is removed.");
            return false;
        }
        planeCount = resources->GetFormatPlaneCount(resourceDesc.Format);

        D3D12_HEAP_PROPERTIES heapProp{};
        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
        
        bool isShared = false;
        if(HasFlags(desc.sharedResourceFlags, SharedResourceFlags::Shared)){
            heapFlags |= D3D12_HEAP_FLAG_SHARED;
            isShared = true;
        }
        if(HasFlags(desc.sharedResourceFlags, SharedResourceFlags::Shared_CrossAdapter)){
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
            heapFlags |= D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
        }
        if(desc.isTiled){
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
        }

        D3D12_CLEAR_VALUE clearValue = ConvertClearValue(desc);

        // 虚拟显存，后续使用 BingTextureMemory 绑定物理显存
        if(desc.isVirtual) return createSuccess();

        // 创建资源
        HRESULT hr = S_OK;
        if(desc.isTiled){
            hr = m_Context.device->CreateReservedResource(
                &resourceDesc, 
                ConvertResourceStates(desc.initialState),
                desc.useClearValue ? &clearValue : nullptr, 
                IID_PPV_ARGS(resource.GetAddressOf()));
        }
        else{
            heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
            hr = m_Context.device->CreateCommittedResource(
                &heapProp, 
                heapFlags, 
                &resourceDesc, 
                ConvertResourceStates(desc.initialState),
                desc.useClearValue ? &clearValue : nullptr, 
                IID_PPV_ARGS(resource.GetAddressOf()));
        }

        if(FAILED(hr)){
            std::string msg = std::format("Failed to create texture {}, error msg: {}", 
                DebugNameToString(desc.debugName), GetHRErrorMessage(hr));
            m_Context.Error(msg);
            return false;
        }

        // 创建共享句柄
        if(isShared){
            hr = m_Context.device->CreateSharedHandle(
                resource.Get(), nullptr, GENERIC_ALL, nullptr, &sharedHandle);
            if(FAILED(hr)){
                std::string msg = std::format("Failed to create shared handle for texture {}, error msg: {}", 
                DebugNameToString(desc.debugName), GetHRErrorMessage(hr));
                m_Context.Error(msg);
                return false;
            }
        }

        if(!desc.debugName.empty()){
            auto name = Utility::UTF8ToWString(desc.debugName);
            resource->SetName(name.c_str());
        }

        return createSuccess();
    }

    void Texture::Create(TextureDesc desc, ID3D12Resource *r)
    {
        assert(r != nullptr);

        resourceDesc = r->GetDesc();        
        if(!desc.debugName.empty()){
            auto name = Utility::UTF8ToWString(desc.debugName);
            r->SetName(name.c_str());
        }
        m_Desc = std::move(desc);

        resource = r;

        m_Context.stateTracker->RegisterTexture(this);
    }

    void Texture::Destroy()
    {

        m_Context.stateTracker->UnregisterTexture(this);
        if(auto resources = m_Resources.lock()){
            auto releaseDescriptor = [&](const auto& views, auto& heap) {
                for(const auto& [bindingKey, index] : views){
                    heap.ReleaseDescriptor(index);
                }
            };
            // 销毁时归还所有描述符
            releaseDescriptor(m_RenderTargetViews, resources->renderTargetViewHeap);
            releaseDescriptor(m_DepthStencilViews, resources->depthStencilViewHeap);
            releaseDescriptor(m_CustomSRVs, resources->shaderResourceViewHeap);
            releaseDescriptor(m_CustomUAVs, resources->shaderResourceViewHeap);
            for(const auto& index : m_ClearMipLevelUAVs){
                if(index != c_InvalidDescriptorIndex){
                    resources->shaderResourceViewHeap.ReleaseDescriptor(index);
                }
            }
        }

        resource = nullptr;
        heap = nullptr;
        m_RenderTargetViews.clear();
        m_DepthStencilViews.clear();
        m_CustomSRVs.clear();
        m_CustomUAVs.clear();
        m_ClearMipLevelUAVs.clear();
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
        auto resources = m_Resources.lock();
        if(resources == nullptr) 
            return nullptr;

        uint64_t descriptor{};
        subresources = subresources.Resolve(m_Desc, false);
        TextureBindingKey key = TextureBindingKey(subresources, format);
        uint32_t descriptorIndex{};
        switch (objType)
        {
        case ObjectTypes::D3D12_ShaderResourceViewCpuDescriptor:
        case ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor:{
            auto& descriptorHeap = resources->shaderResourceViewHeap;

            if (auto found = m_CustomSRVs.find(key); found == m_CustomSRVs.end()) {
                descriptorIndex = descriptorHeap.AllocateDescriptor();
                m_CustomSRVs[key] = descriptorIndex;

                const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptorHeap.GetCpuHandle(descriptorIndex);
                CreateSRV(cpuHandle.ptr, format, dimension, subresources);
                descriptorHeap.CopyToShaderVisibleHeap(descriptorIndex);
            }
            else {
                descriptorIndex = found->second;
            }

            descriptor = objType == ObjectTypes::D3D12_ShaderResourceViewCpuDescriptor ?
                descriptorHeap.GetCpuHandle(descriptorIndex).ptr :
                descriptorHeap.GetGpuHandle(descriptorIndex).ptr;
            break;
        }
        case ObjectTypes::D3D12_UnorderedAccessViewCpuDescriptor:
        case ObjectTypes::D3D12_UnorderedAccessViewGpuDescriptor:{
            auto& descriptorHeap = resources->shaderResourceViewHeap;

            if (auto found = m_CustomUAVs.find(key); found == m_CustomUAVs.end()) {
                descriptorIndex = descriptorHeap.AllocateDescriptor();
                m_CustomUAVs[key] = descriptorIndex;

                const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptorHeap.GetCpuHandle(descriptorIndex);
                CreateUAV(cpuHandle.ptr, format, dimension, subresources);
                descriptorHeap.CopyToShaderVisibleHeap(descriptorIndex);
            }
            else {
                descriptorIndex = found->second;
            }

            descriptor = objType == ObjectTypes::D3D12_UnorderedAccessViewCpuDescriptor ?
                descriptorHeap.GetCpuHandle(descriptorIndex).ptr :
                descriptorHeap.GetGpuHandle(descriptorIndex).ptr;
            break;
        }
        case ObjectTypes::D3D12_RenderTargetViewDescriptor:{
            auto& descriptorHeap = resources->renderTargetViewHeap;

            if (auto found = m_RenderTargetViews.find(key); found == m_RenderTargetViews.end()) {
                descriptorIndex = descriptorHeap.AllocateDescriptor();
                m_RenderTargetViews[key] = descriptorIndex;

                const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptorHeap.GetCpuHandle(descriptorIndex);
                CreateRTV(cpuHandle.ptr, format, subresources);
            }
            else {
                descriptorIndex = found->second;
            }

            descriptor = descriptorHeap.GetCpuHandle(descriptorIndex).ptr;
            break;
        }
        case ObjectTypes::D3D12_DepthStencilViewDescriptor:{
            auto& descriptorHeap = resources->depthStencilViewHeap;

            if (auto found = m_DepthStencilViews.find(key); found == m_DepthStencilViews.end()) {
                descriptorIndex = descriptorHeap.AllocateDescriptor();
                m_DepthStencilViews[key] = descriptorIndex;

                const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptorHeap.GetCpuHandle(descriptorIndex);
                CreateDSV(cpuHandle.ptr, subresources, isReadOnlyDSV);
            }
            else {
                descriptorIndex = found->second;
            }

            descriptor = descriptorHeap.GetCpuHandle(descriptorIndex).ptr;
            break;
        }
        default:
            return Object{nullptr};
        }

        return Object{descriptor};
    }
    
    void Texture::CreateSRV(size_t descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const
    {
        subresources = subresources.Resolve(m_Desc, false);

        dimension = dimension == TextureDimension::Unknown ? m_Desc.dimension : dimension;
        format = format == Format::UNKNOWN ? m_Desc.format : format;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = GetDxgiFormatMapping(format).srvFormat;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

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

        m_Context.device->CreateShaderResourceView(resource.Get(), &srvDesc, {descriptor});
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

        m_Context.device->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, {descriptor});
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

        m_Context.device->CreateRenderTargetView(resource.Get(), &rtvDesc, {descriptor});
    }

    void Texture::CreateDSV(size_t descriptor, TextureSubresourceSet subresources, bool isReadOnly) const
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

        m_Context.device->CreateDepthStencilView(resource.Get(), &dsvDesc, {descriptor});
    }

    uint32_t Texture::GetClearMipLevelUAV(uint32_t mipLevel)
    {
        if(!m_Desc.isUAV){
            m_Context.Error("Texture is no UAV.");
        }

        uint32_t descriptorIndex = m_ClearMipLevelUAVs[mipLevel];
        if(auto resources = m_Resources.lock(); 
            resources != nullptr && descriptorIndex == c_InvalidDescriptorIndex){
            descriptorIndex = resources->shaderResourceViewHeap.AllocateDescriptor();
            assert(descriptorIndex != c_InvalidDescriptorIndex);
            auto handle = resources->shaderResourceViewHeap.GetCpuHandle(descriptorIndex);
            TextureSubresourceSet subresources{mipLevel, 1, 0, TextureSubresourceSet::AllArraySlices};
            CreateUAV(handle.ptr, Format::UNKNOWN, TextureDimension::Unknown, subresources);
            resources->shaderResourceViewHeap.CopyToShaderVisibleHeap(descriptorIndex);
            m_ClearMipLevelUAVs[mipLevel] = descriptorIndex;
        }

        return descriptorIndex;
    }


} // namespace DSM::D3D12

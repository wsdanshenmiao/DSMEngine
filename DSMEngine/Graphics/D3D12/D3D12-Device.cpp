#include "D3D12-Device.h"
#include "D3D12-Texture.h"


namespace DSM::D3D12{
    TextureHandle Device::CreateTexture(const TextureDesc &desc)
    {
        
        D3D12_RESOURCE_DESC resourceDesc = Texture::ConvertTextureDesc(desc);

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

        D3D12_CLEAR_VALUE clearValue = Texture::ConvertClearValue(desc);

        Texture* texture = new Texture{m_Context, m_Resources, desc};
        auto& resource = texture->resource;

        // 虚拟显存，后续使用 BingTextureMemory 绑定物理显存
        if(desc.isVirtual) return TextureHandle{texture};

        // 创建资源
        HRESULT hr = S_OK;
        if(desc.isTiled){
            hr = m_Context.m_Device->CreateReservedResource(
                &resourceDesc, ConvertResourceStates(desc.initialState),
                &clearValue, IID_PPV_ARGS(resource.GetAddressOf()));
        }
        else{
            heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
            hr = m_Context.m_Device->CreateCommittedResource(
                &heapProp, heapFlags, 
                &resourceDesc, ConvertResourceStates(desc.initialState),
                &clearValue, IID_PPV_ARGS(resource.GetAddressOf()));
        }

        if(FAILED(hr)){
            std::string msg = std::format("Failed to create texture {}, error msg: {}", 
                DebugNameToString(desc.debugName), GetErrorMessage(hr));
            m_Context.Error(msg);
            return TextureHandle{nullptr};
        }

        // 创建共享句柄
        if(isShared){
            hr = m_Context.m_Device->CreateSharedHandle(
                resource.Get(), nullptr, GENERIC_ALL, nullptr, &texture->sharedHandle);
            return TextureHandle{nullptr};
        }

        if(!desc.debugName.empty()){
            auto name = Utility::UTF8ToWString(desc.debugName);
            resource->SetName(name.c_str());
        }

        return TextureHandle{texture};
    }
}
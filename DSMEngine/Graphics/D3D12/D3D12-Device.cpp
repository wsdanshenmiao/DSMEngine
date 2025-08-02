#include "D3D12-Device.h"
#include "D3D12-Texture.h"
#include "D3D12-Heap.h"
#include "D3D12-Buffer.h"
#include <format>

namespace DSM::D3D12{
    
    //////////////////////////////////////////////////////////////////////////
    // Heap
    //////////////////////////////////////////////////////////////////////////
    HeapHandle Device::CreateHeap(const HeapDesc &d)
    {
        D3D12_HEAP_DESC heapDesc{};
        heapDesc.SizeInBytes = d.capacity;
        heapDesc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
        heapDesc.Properties.VisibleNodeMask = 1;
        heapDesc.Properties.CreationNodeMask = 1;
        
        switch (d.type)
        {
        case HeapType::Default:
            heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            break;
        case HeapType::Upload:
            heapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
            break;
        case HeapType::Readback:
            heapDesc.Properties.Type = D3D12_HEAP_TYPE_READBACK;
            break;
        default:
            assert(!"Invalid heap type.");
            break;
        }

        heapDesc.Flags = m_Options.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_1 ?
            D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES : D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

        RefPtr<ID3D12Heap> d3dHeap{};
        auto hr = m_Context.m_Device->CreateHeap(&heapDesc, IID_PPV_ARGS(d3dHeap.GetAddressOf()));

        if(FAILED(hr)){
            std::string msg = std::format("Failed to create heap {}, error msg: {}", 
                DebugNameToString(d.debugName), GetErrorMessage(hr));
            m_Context.Error(msg);
            return HeapHandle{nullptr};
        }

        if(!d.debugName.empty()){
            std::wstring name = Utility::UTF8ToWString(d.debugName);
            d3dHeap->SetName(name.c_str());
        }

        Heap* heap = new Heap(d, d3dHeap.Get());
        return HeapHandle{heap};
    }


    
    //////////////////////////////////////////////////////////////////////////
    // Texture
    //////////////////////////////////////////////////////////////////////////
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

        Texture* texture = new Texture{m_Context, m_Resources, desc, resourceDesc};
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
            delete texture;
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
    
    MemoryRequirements Device::GetTextureMemoryRequirements(ITexture *texture)
    {
        Texture* tex = Utility::CheckedCast<Texture*>(texture);

        D3D12_RESOURCE_ALLOCATION_INFO info = m_Context.m_Device->GetResourceAllocationInfo(1, 1, &tex->resourceDesc);
        MemoryRequirements ret{};
        ret.size = info.SizeInBytes;
        ret.alignment = info.Alignment;
        return ret;
    }

    bool Device::BindTextureMemory(ITexture *_texture, IHeap *_heap, uint64_t offset)
    {
        assert(_texture != nullptr && _heap != nullptr);

        Texture* texture = Utility::CheckedCast<Texture*>(_texture);
        Heap* heap = Utility::CheckedCast<Heap*>(_heap);

        if(heap == nullptr || !texture->GetDesc().isVirtual) return false;

        D3D12_CLEAR_VALUE clearValue = Texture::ConvertClearValue(texture->GetDesc());
        auto hr = m_Context.m_Device->CreatePlacedResource(
            heap->GetHeap(), 
            offset, 
            &texture->resourceDesc, 
            ConvertResourceStates(texture->GetDesc().initialState),
            texture->GetDesc().useClearValue ? &clearValue : nullptr,
            IID_PPV_ARGS(texture->resource.GetAddressOf()));

        if(FAILED(hr)){
            std::string msg = std::format("Failed to create placed texture {}, error msg: {}", 
                DebugNameToString(texture->GetDesc().debugName), GetErrorMessage(hr));
            m_Context.Error(msg);
            return false;
        }

        texture->heap = heap;
    }
    
    TextureHandle Device::CreateHandleForNativeTexture(ObjectType objectType, Object texture, const TextureDesc &desc)
    {
        if(texture.pointer != nullptr) return TextureHandle{nullptr};
        if(objectType != ObjectTypes::D3D12_Resource) return TextureHandle{nullptr};
        
        ID3D12Resource* resource = static_cast<ID3D12Resource*>(texture.pointer);

        Texture* tex = new Texture{m_Context, m_Resources, desc, resource->GetDesc()};
        tex->resource = resource;

        return TextureHandle{tex};
    }
 
    
    
    //////////////////////////////////////////////////////////////////////////
    // Buffer
    //////////////////////////////////////////////////////////////////////////
    BufferHandle Device::CreateBuffer(const BufferDesc &d)
    {
        BufferDesc desc = d;
        if(desc.isConstantBuffer)
            desc.byteSize = Utility::Align(d.byteSize, 255llu);
    
        Buffer* buffer = new Buffer{m_Context, m_Resources, desc};
        if(desc.isVolatile) return BufferHandle{buffer};

        auto& resourceDesc = buffer->resourceDesc;
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
            return BufferHandle{buffer};

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

        switch (desc.cpuAccess)
        {
        case CpuAccessMode::None:{
            heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
            resourceState = ConvertResourceStates(desc.initialState);
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
        if ((desc.cpuAccess == CpuAccessMode::Read) && (desc.initialState == ResourceStates::ResolveDest))
        {
            heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
            heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
            heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
            resourceState = D3D12_RESOURCE_STATE_COMMON;
        }

        auto hr = m_Context.m_Device->CreateCommittedResource(
            &heapProp, heapFlags, 
            &resourceDesc, resourceState,
            nullptr, IID_PPV_ARGS(buffer->resource.GetAddressOf()));

        if (FAILED(hr))
        {
            std::string msg = std::format("CreateCommittedResource call failed for buffer: {}, error msg: {}.",
                DebugNameToString(desc.debugName), GetErrorMessage(hr));
            m_Context.Error(msg);
            delete buffer;
            return BufferHandle{nullptr};
        }
        
        return BufferHandle{buffer};
    }
}
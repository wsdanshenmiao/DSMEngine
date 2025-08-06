#include "D3D12-Device.h"
#include "D3D12-Texture.h"
#include "D3D12-Heap.h"
#include "D3D12-Buffer.h"
#include "D3D12-Sampler.h"
#include "D3d12-FrameBuffer.h"
#include "D3D12-ResourceBindings.h"
#include "D3D12-Shader.h"
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
        return true;
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
            std::string msg = std::format("CreateCommittedResource call failed for buffer {}, error msg: {}.",
                DebugNameToString(desc.debugName), GetErrorMessage(hr));
            m_Context.Error(msg);
            delete buffer;
            return BufferHandle{nullptr};
        }
        
        return BufferHandle{buffer};
    }
    
    void *Device::MapBuffer(IBuffer *_buffer, CpuAccessMode cpuAccess)
    {
        assert(_buffer != nullptr);

        Buffer* buffer = Utility::CheckedCast<Buffer*>(_buffer);

        if(buffer->lastUseFence != nullptr){
            WaitForFence(buffer->lastUseFence, buffer->lastUseFenceValue, m_FenceEvent);
            buffer->lastUseFence = nullptr;
        }

        D3D12_RANGE range{};
        if(cpuAccess == CpuAccessMode::Read){
            range = {0, buffer->GetDesc().byteSize};
        }
        else{
            range = {0, 0};
        }

        void* mappedData = nullptr;
        auto hr = buffer->resource->Map(0, &range, &mappedData);
        if(FAILED(hr)){
            std::string msg = std::format("Map call failed for buffer {}, error msg: {}",
                DebugNameToString(buffer->GetDesc().debugName), GetErrorMessage(hr));
            m_Context.Error(msg);
            return nullptr;
        }

        return mappedData;
    }
    
    void Device::UnmapBuffer(IBuffer *_buffer)
    {
        assert(_buffer != nullptr);
        Buffer* buffer = Utility::CheckedCast<Buffer*>(_buffer);
        buffer->resource->Unmap(0, nullptr);
    }
    
    MemoryRequirements Device::GetBufferMemoryRequirements(IBuffer *_buffer)
    {
        Buffer* buffer = Utility::CheckedCast<Buffer*>(_buffer);

        auto info = m_Context.m_Device->GetResourceAllocationInfo(1, 1, &buffer->resourceDesc);
        MemoryRequirements memReq{};
        memReq.size = info.SizeInBytes;
        memReq.alignment = info.Alignment;
        return memReq;
    }
    
    bool Device::BindBufferMemory(IBuffer *_buffer, IHeap *_heap, uint64_t offset)
    {
        assert(_buffer != nullptr && _heap != nullptr);

        Buffer* buffer = Utility::CheckedCast<Buffer*>(_buffer);
        Heap* heap = Utility::CheckedCast<Heap*>(_heap);

        if(buffer->resource != nullptr || !buffer->GetDesc().isVirtual) return false;

        D3D12_RESOURCE_STATES resourceState = ConvertResourceStates(buffer->GetDesc().initialState);
        if(resourceState != D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
            resourceState = D3D12_RESOURCE_STATE_COMMON;

        auto hr = m_Context.m_Device->CreatePlacedResource(
            heap->GetHeap(), offset,
            &buffer->resourceDesc, resourceState, 
            nullptr, IID_PPV_ARGS(buffer->resource.GetAddressOf()));

        if(FAILED(hr)){
            std::string msg = std::format("Failed to create placed buffer {}, error msg: {}.",
                DebugNameToString(buffer->GetDesc().debugName), GetErrorMessage(hr));
            m_Context.Error(msg);
            return false;
        }

        buffer->heap = heap;
        return true;
    }

    BufferHandle Device::CreateHandleForNativeBuffer(ObjectType objectType, Object _buffer, const BufferDesc &desc)
    {
        if(_buffer.pointer == nullptr || objectType != ObjectTypes::D3D12_Resource)
            return BufferHandle{nullptr};

        ID3D12Resource* resource = static_cast<ID3D12Resource*>(_buffer);
        Buffer* buffer = new Buffer{m_Context, m_Resources, desc};
        buffer->resource = resource;
        
        return BufferHandle{buffer};
    }

    
    //////////////////////////////////////////////////////////////////////////
    // Shader
    //////////////////////////////////////////////////////////////////////////
    ShaderHandle Device::CreateShader(const ShaderDesc &d, const void *binary, size_t binarySize)
    {
        if(binarySize == 0 || binary == nullptr) return ShaderHandle{nullptr};

        Shader* shader = new Shader{d};
        shader->bytecode.resize(binarySize);
        std::memcpy(shader->bytecode.data(), binary, binarySize);


        return ShaderHandle{shader};
    }

    ShaderLibraryHandle Device::CreateShaderLibrary(const void *binary, size_t binarySize)
    {
        if (binarySize == 0 || binary == nullptr) return ShaderLibraryHandle{nullptr};

        ShaderLibrary* library = new ShaderLibrary();
        library->bytecode.resize(binarySize);
        std::memcpy(library->bytecode.data(), binary, binarySize);
        return ShaderLibraryHandle{library};
    }

    //////////////////////////////////////////////////////////////////////////
    // Sampler
    //////////////////////////////////////////////////////////////////////////
    SamplerHandle Device::CreateSampler(const SamplerDesc &d)
    {
        Sampler* sampler = new Sampler{m_Context, d};
        return SamplerHandle{sampler};
    }
    
    
    
    
    //////////////////////////////////////////////////////////////////////////
    // InputLayout
    //////////////////////////////////////////////////////////////////////////
    InputLayoutHandle Device::CreateInputLayout(const VertexAttributeDesc *desc, uint32_t attributeCount, IShader *vertexShader)
    {
        InputLayout* layout = new InputLayout{};
        layout->attributes.resize(attributeCount);

        for(int i = 0; i < attributeCount; ++i){
            VertexAttributeDesc& attr = layout->attributes[i];

            attr = desc[i];

            const auto& formatMapping = GetDxgiFormatMapping(attr.format);
            const auto& formatInfo = GetFormatInfo(attr.format);

            // 元素语义的索引，一个 matrix 的四行分别对应 0，1，2，3
            for(int j = 0; j < attr.arraySize; ++j){
                D3D12_INPUT_ELEMENT_DESC elementDesc{};
                elementDesc.SemanticName = attr.name.c_str();
                elementDesc.SemanticIndex = j;
                elementDesc.Format = formatMapping.srvFormat;
                elementDesc.InputSlot = attr.bufferIndex;
                elementDesc.AlignedByteOffset = attr.offset + j * formatInfo.bytesPerBlock;
                elementDesc.InputSlotClass = attr.isInstanced ? 
                    D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                elementDesc.InstanceDataStepRate = attr.isInstanced ? 1 : 0;    // 使用一个实例绘制的实例数
                
                layout->inputElements.push_back(std::move(elementDesc));
            }

            // 每个槽位只绑定一个数据
            if(auto it = layout->elementStride.find(attr.bufferIndex); it == layout->elementStride.end()){
                layout->elementStride.emplace(attr.bufferIndex, attr.elementStride);
            }
            else{
                assert(layout->elementStride[attr.bufferIndex] == attr.elementStride);
            }
        }

        return InputLayoutHandle(layout);
    }
    
    GraphicsAPI Device::GetGraphicsAPI()
    {
        return GraphicsAPI::D3D12;
    }
    

    
    //////////////////////////////////////////////////////////////////////////
    // FrameBuffer
    //////////////////////////////////////////////////////////////////////////
    FramebufferHandle Device::CreateFramebuffer(const FramebufferDesc &desc)
    {
        Framebuffer* framebuffer = new Framebuffer{m_Resources, desc};
        const FramebufferAttachment& ds = desc.depthAttachment;

        if(!desc.colorAttachments.Empty()){
            const TextureDesc& texDesc = desc.colorAttachments[0].texture->GetDesc();
            framebuffer->rtWidth = texDesc.width;
            framebuffer->rtHeight = texDesc.height;
        }
        else if(ds.Valid()){
            const TextureDesc& texDesc = ds.texture->GetDesc();
            framebuffer->rtWidth = texDesc.width;
            framebuffer->rtHeight = texDesc.height;
        }

        for(auto& rt : desc.colorAttachments){
            const TextureDesc& texDesc = rt.texture->GetDesc();
            assert(framebuffer->rtWidth == texDesc.width);
            assert(framebuffer->rtHeight == texDesc.height);
            framebuffer->textures.PushBack(TextureHandle{rt.texture});

            Object rtv = rt.texture->GetNativeView(
                ObjectTypes::D3D12_RenderTargetViewDescriptor, 
                rt.format, rt.subresources, texDesc.dimension, rt.isReadOnly);
            
            uint32_t index = m_Resources.renderTargetViewHeap.GetOffsetOfCpuHandle(rtv.integer);
            framebuffer->RTVs.PushBack(index);
        }

        if(ds.Valid()){
            const TextureDesc& texDesc = ds.texture->GetDesc();
            assert(framebuffer->rtWidth == texDesc.width);
            assert(framebuffer->rtHeight == texDesc.height);
            framebuffer->textures.PushBack(TextureHandle{ds.texture});

            Object dsv = ds.texture->GetNativeView(
                ObjectTypes::D3D12_DepthStencilViewDescriptor, 
                ds.format, ds.subresources, texDesc.dimension, ds.isReadOnly);
            
            framebuffer->DSV = m_Resources.depthStencilViewHeap.GetOffsetOfCpuHandle(dsv.integer);
        }

        return FramebufferHandle{framebuffer};
    }

    
    //////////////////////////////////////////////////////////////////////////
    // Resource binding
    //////////////////////////////////////////////////////////////////////////    
    BindingLayoutHandle Device::CreateBindingLayout(const BindingLayoutDesc &desc)
    {
        return BindingLayoutHandle{new BindingLayout{desc}};
    }
    
    BindingLayoutHandle Device::CreateBindlessLayout(const BindlessLayoutDesc &desc)
    {
        return BindingLayoutHandle{new BindlessLayout{desc}};
    }
    
    BindingSetHandle Device::CreateBindingSet(const BindingSetDesc &desc, IBindingLayout *layout)
    {
        auto bindingLayout = Utility::CheckedCast<BindingLayout*>(layout);
        return BindingSetHandle{new BindingSet{m_Context, m_Resources,desc, bindingLayout}};
    }

    DescriptorTableHandle Device::CreateDescriptorTable(IBindingLayout *layout)
    {
        return DescriptorTableHandle(new DescriptorTable{m_Resources});
    }




    IMessageCallback *Device::GetMessageCallback()
    {
        return m_Context.m_MessageCallback;
    }
}

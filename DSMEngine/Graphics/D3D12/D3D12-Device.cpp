#include "D3D12-Device.h"
#include "D3D12-Texture.h"
#include "D3D12-Heap.h"
#include "D3D12-Buffer.h"
#include "D3D12-Sampler.h"
#include "D3d12-FrameBuffer.h"
#include "D3D12-ResourceBindings.h"
#include "D3D12-Shader.h"
#include "D3D12-PipelineState.h"
#include <format>

// 通过栅栏值的偏移来直接获取队列的类型
#define QUEUE_TYPE_MOVEBITS 56

namespace DSM::D3D12{

    //////////////////////////////////////////////////////////////////////////
    // DeviceResources
    //////////////////////////////////////////////////////////////////////////
    DeviceResources::DeviceResources(const Context &context, const DeviceDesc &desc)
        :m_Context(context), renderTargetViewHeap(context), depthStencilViewHeap(context),
        shaderResourceViewHeap(context), samplerHeap(context) ,
        timerQueries(desc.maxTimerQueries, true) {}
    
    uint8_t DeviceResources::GetFormatPlaneCount(DXGI_FORMAT format)
    {
        uint8_t planeCount = 0;
        if(auto it = m_DxgiFormatPlaneCounts.find(format); it != m_DxgiFormatPlaneCounts.end()){
            planeCount = it->second == 255 ? 0 : it->second;
        }
        else{
            D3D12_FEATURE_DATA_FORMAT_INFO info = {.Format = format, .PlaneCount = 1};
            if(FAILED(m_Context.m_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &info, sizeof(info)))){
                planeCount = 255;
            }
            else{
                planeCount = info.PlaneCount;
            }
        }
        return planeCount;
    }


    
    //////////////////////////////////////////////////////////////////////////
    // CommandQueue
    //////////////////////////////////////////////////////////////////////////
    CommandQueue::CommandQueue(Device &device, CommandQueueType queueType)
        :m_Device(device),
        m_QueueType(queueType),
        m_LastCompletedFenceValue((uint64_t)queueType << QUEUE_TYPE_MOVEBITS),
        m_NextFenceValue(m_LastCompletedFenceValue | 1)
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        switch (queueType) {
        case CommandQueueType::Graphics:
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
        case CommandQueueType::Compute:
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
        case CommandQueueType::Copy:
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY; break;
        default:
            assert(!"Invalid command queue type.");
            return;
        }
        ID3D12Device* d3ddevice = m_Device.GetContext().m_Device;
        auto hr = d3ddevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_CommandQueue.GetAddressOf()));

        hr = d3ddevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_Fence.GetAddressOf()));
        assert(SUCCEEDED(hr));
        m_Fence->SetName(L"CommandQueue::m_Fence");
        m_Fence->Signal(m_LastCompletedFenceValue);

        m_FenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
        assert(m_FenceEventHandle != nullptr);
    }

    uint64_t CommandQueue::IncrementFence()
    {
        std::lock_guard<std::mutex> guard{m_FenceMutex};

        m_CommandQueue->Signal(m_Fence.Get(), m_NextFenceValue);
        return m_NextFenceValue++;
    }

    bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
    {
        std::lock_guard<std::mutex> guard{m_FenceMutex};

        // 更新栅栏值
        if (m_LastCompletedFenceValue < fenceValue) {
            m_LastCompletedFenceValue = std::max(m_LastCompletedFenceValue, m_Fence->GetCompletedValue());
        }

        return fenceValue <= m_LastCompletedFenceValue;
    }

    void CommandQueue::StallForFence(uint64_t fenceValue)
    {
        CommandQueue* producer = m_Device.GetQueue((CommandQueueType)(fenceValue >> QUEUE_TYPE_MOVEBITS));
        m_CommandQueue->Wait(producer->m_Fence.Get(), fenceValue);
    }

    void CommandQueue::StallForProducer(CommandQueue &producer)
    {
        uint64_t nextFenceValue = producer.GetNextFenceValue();
        assert(nextFenceValue > 0);
        m_CommandQueue->Wait(producer.m_Fence.Get(), nextFenceValue - 1);
    }

    void CommandQueue::WaitForFence(uint64_t fenceValue)
    {
        // 已经执行过了则无需等待
        if (IsFenceComplete(fenceValue)) return;

        // 等待 GPU 执行到栅栏点
        std::lock_guard<std::mutex> guard{m_EventMutex};
        m_Fence->SetEventOnCompletion(fenceValue, m_FenceEventHandle);
        WaitForSingleObject(m_FenceEventHandle, INFINITE);
        m_LastCompletedFenceValue = fenceValue;
    }

    uint64_t CommandQueue::ExecuteCommandList(ID3D12CommandList *list)
    {
        assert(list != nullptr);
    
        std::lock_guard<std::mutex> guard{m_EventMutex};

        m_CommandQueue->ExecuteCommandLists(1, &list);
        m_CommandQueue->Signal(m_Fence.Get(), m_NextFenceValue);
        
        return m_NextFenceValue++;
    }




    //////////////////////////////////////////////////////////////////////////
    // Device
    //////////////////////////////////////////////////////////////////////////

    Object Device::GetNativeObject(ObjectType type)
    {
        switch (type) {
        case ObjectTypes::D3D12_Device:
            return Object(m_Context.m_Device);
        case ObjectTypes::D3D12_CommandQueue:
            return Object(GetQueue(CommandQueueType::Graphics)->GetCommandQueue());
        default:
            return nullptr;
        }
    }

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

    void Device::GetTextureTiling(ITexture *texture, uint32_t *numTiles, PackedMipDesc *desc, TileShape *_tileShape, uint32_t *_subresourceTilingsNum, SubresourceTiling *_subresourceTilings)
    {
        ID3D12Resource* resource = texture->GetNativeObject(ObjectTypes::D3D12_Resource);
        D3D12_PACKED_MIP_INFO packedMipInfo{};
        D3D12_TILE_SHAPE tileShape{};
        D3D12_SUBRESOURCE_TILING subresourceTilings[16];

        m_Context.m_Device->GetResourceTiling(
            resource, numTiles, desc == nullptr ? nullptr : &packedMipInfo, 
            _tileShape == nullptr ? nullptr : &tileShape, 
            _subresourceTilingsNum, 0, subresourceTilings);
        
        if(desc != nullptr){
            desc->numPackedMips = packedMipInfo.NumPackedMips;
            desc->numStandardMips = packedMipInfo.NumStandardMips;
            desc->numTilesForPackedMips = packedMipInfo.NumTilesForPackedMips;
            desc->startTileIndexInOverallResource = packedMipInfo.StartTileIndexInOverallResource;
        }
        if(_tileShape != nullptr){
            _tileShape->widthInTexels = tileShape.WidthInTexels;
            _tileShape->heightInTexels = tileShape.HeightInTexels;
            _tileShape->depthInTexels = tileShape.DepthInTexels;
        }
        for(uint32_t i = 0; i < *numTiles; ++i){
            _subresourceTilings[i].widthInTiles = subresourceTilings[i].WidthInTiles;
            _subresourceTilings[i].heightInTiles = subresourceTilings[i].HeightInTiles;
            _subresourceTilings[i].depthInTiles = subresourceTilings[i].DepthInTiles;
            _subresourceTilings[i].startTileIndexInOverallResource = subresourceTilings[i].StartTileIndexInOverallResource;
        }
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

        if(!desc.colorAttachments.empty()){
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
            framebuffer->textures.push_back(TextureHandle{rt.texture});

            Object rtv = rt.texture->GetNativeView(
                ObjectTypes::D3D12_RenderTargetViewDescriptor, 
                rt.format, rt.subresources, texDesc.dimension, rt.isReadOnly);
            
            uint32_t index = m_Resources.renderTargetViewHeap.GetOffsetOfCpuHandle(rtv.integer);
            framebuffer->RTVs.push_back(index);
        }

        if(ds.Valid()){
            const TextureDesc& texDesc = ds.texture->GetDesc();
            assert(framebuffer->rtWidth == texDesc.width);
            assert(framebuffer->rtHeight == texDesc.height);
            framebuffer->textures.push_back(TextureHandle{ds.texture});

            Object dsv = ds.texture->GetNativeView(
                ObjectTypes::D3D12_DepthStencilViewDescriptor, 
                ds.format, ds.subresources, texDesc.dimension, ds.isReadOnly);
            
            framebuffer->DSV = m_Resources.depthStencilViewHeap.GetOffsetOfCpuHandle(dsv.integer);
        }

        return FramebufferHandle{framebuffer};
    }


    //////////////////////////////////////////////////////////////////////////
    // Pipeline State
    //////////////////////////////////////////////////////////////////////////
    GraphicsPipelineHandle Device::CreateGraphicsPipeline(const GraphicsPipelineDesc &desc, IFramebuffer *fb)
    {
        RefPtr<RootSignature> rootSig = GetRootSignature(desc.bindingLayouts, desc.inputLayout != nullptr);

        // 创建 PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = rootSig->rootSignature.Get();
        
        // 设置 Shader
        auto setShader = [](ShaderHandle shader, auto& psoShader){
            if(shader != nullptr){
                const void* shaderBytecode = nullptr;
                size_t shaderBytecodeSize = 0;
                shader->GetBytecode(&shaderBytecode, &shaderBytecodeSize);
                psoShader = { shaderBytecode, shaderBytecodeSize };
            }
        };
        setShader(desc.VS, psoDesc.VS);
        setShader(desc.HS, psoDesc.HS);
        setShader(desc.DS, psoDesc.DS);
        setShader(desc.GS, psoDesc.GS);
        setShader(desc.PS, psoDesc.PS);

        // 设置状态
        const auto& fbInfo = fb->GetFramebufferInfo();
        psoDesc.BlendState = ConvertBlendState(desc.renderState.blendState);
        psoDesc.DepthStencilState = ConvertDepthStencilState(desc.renderState.depthStencilState);
        if ((psoDesc.DepthStencilState.DepthEnable || psoDesc.DepthStencilState.StencilEnable) && 
            fbInfo.depthFormat == Format::UNKNOWN)
        {
            psoDesc.DepthStencilState.DepthEnable = FALSE;
            psoDesc.DepthStencilState.StencilEnable = FALSE;
            GetMessageCallback()->Message(MessageSeverity::Warning, "DepthEnable or stencilEnable is true, but no depth target is bound");
        }
        psoDesc.RasterizerState = ConvertRasterizerState(desc.renderState.rasterState);


        switch (desc.primType) {
        case PrimitiveType::PointList:
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            break;
        case PrimitiveType::LineList:
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            break;
        case PrimitiveType::TriangleList:
        case PrimitiveType::TriangleStrip:
        case PrimitiveType::TriangleFan:
        case PrimitiveType::TriangleListWithAdjacency:
        case PrimitiveType::TriangleStripWithAdjacency:
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            break;
        case PrimitiveType::PatchList:
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
            break;
        default:
            m_Context.Error("PrimitiveType unsupported by this device.");
            return nullptr;
        }

        psoDesc.DSVFormat = GetDxgiFormatMapping(fbInfo.depthFormat).rtvFormat;
        psoDesc.SampleDesc.Count = fbInfo.sampleCount;
        psoDesc.SampleDesc.Quality = fbInfo.sampleQuality;
        psoDesc.SampleMask = ~0u;
        psoDesc.NumRenderTargets = static_cast<UINT>(fbInfo.colorFormats.size());
        for(uint32_t i = 0; i < psoDesc.NumRenderTargets; ++i){
            psoDesc.RTVFormats[i] = GetDxgiFormatMapping(fbInfo.colorFormats[i]).rtvFormat;
        }

        InputLayout* inputLayout = Utility::CheckedCast<InputLayout*>(desc.inputLayout.Get());
        psoDesc.InputLayout.NumElements = inputLayout->inputElements.size();
        psoDesc.InputLayout.pInputElementDescs = inputLayout->inputElements.data();

        RefPtr<ID3D12PipelineState> pipelineState{};
        auto hr = m_Context.m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
        if(FAILED(hr)){
            m_Context.Error("Failed to create a graphics pipeline state object.");
            return nullptr;
        }


        return CreateHandleForNativeGraphicsPipeline(rootSig.Get(), pipelineState.Get(), desc, fbInfo);
    }

    ComputePipelineHandle Device::CreateComputePipeline(const ComputePipelineDesc &desc)
    {
        RefPtr<RootSignature> rootSig = GetRootSignature(desc.bindingLayouts, false);

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = rootSig->rootSignature.Get();
        
        if(desc.CS == nullptr) return nullptr;
        const void* shaderBytecode = nullptr;
        size_t shaderBytecodeSize = 0;
        desc.CS->GetBytecode(&shaderBytecode, &shaderBytecodeSize);
        psoDesc.CS = { shaderBytecode, shaderBytecodeSize };

        RefPtr<ID3D12PipelineState> pipelineState{};
        const auto hr = m_Context.m_Device->CreateComputePipelineState(
            &psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));

        if(FAILED(hr)){
            m_Context.Error("Failed to create a compute pipeline state object.");
            return nullptr;
        }

        auto pso = new ComputePipeline{desc};
        pso->pipelineState = pipelineState;
        pso->rootSignature = rootSig;

        return ComputePipelineHandle{pso};
    }

    MeshletPipelineHandle Device::CreateMeshletPipeline(const MeshletPipelineDesc &desc, IFramebuffer *fb)
    {
        assert(fb != nullptr);

        RefPtr<RootSignature> rootSig = GetRootSignature(desc.bindingLayouts, false);

        const auto& fbInfo = fb->GetFramebufferInfo();

        struct PSO_Stream{
            using SubobjectType = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE;
            SubobjectType RootSignature_Type;        ID3D12RootSignature* RootSignature;
            SubobjectType PrimitiveTopology_Type;    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
            SubobjectType AmplificationShader_Type;  D3D12_SHADER_BYTECODE AmplificationShader;
            SubobjectType MeshShader_Type;           D3D12_SHADER_BYTECODE MeshShader;
            SubobjectType PixelShader_Type;          D3D12_SHADER_BYTECODE PixelShader;
            SubobjectType RasterizerState_Type;      D3D12_RASTERIZER_DESC RasterizerState;
            SubobjectType DepthStencilState_Type;    D3D12_DEPTH_STENCIL_DESC DepthStencilState;
            SubobjectType BlendState_Type;           D3D12_BLEND_DESC BlendState;
            SubobjectType SampleDesc_Type;           DXGI_SAMPLE_DESC SampleDesc;
            SubobjectType SampleMask_Type;           UINT SampleMask;
            SubobjectType RenderTargets_Type;        D3D12_RT_FORMAT_ARRAY RenderTargets;
            SubobjectType DSVFormat_Type;            DXGI_FORMAT DSVFormat;
        } psoStream;

        psoStream.RootSignature_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
        psoStream.PrimitiveTopology_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY;
        psoStream.AmplificationShader_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
        psoStream.MeshShader_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;
        psoStream.PixelShader_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
        psoStream.RasterizerState_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER;
        psoStream.DepthStencilState_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL;
        psoStream.BlendState_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND;
        psoStream.SampleDesc_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC;
        psoStream.SampleMask_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK;
        psoStream.RenderTargets_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
        psoStream.DSVFormat_Type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;

        psoStream.RootSignature = rootSig->rootSignature.Get();
        switch (desc.primType) {
        case PrimitiveType::PointList:
            psoStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            break;
        case PrimitiveType::LineList:
            psoStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            break;
        case PrimitiveType::TriangleList:
        case PrimitiveType::TriangleStrip:
            psoStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            break;
        case PrimitiveType::PatchList:
            m_Context.Error("Unsupported primitive topology for meshlets");
            return nullptr;
        default:
            assert(!"Invalid primitive type.");
            return nullptr;
        } 
        
        auto setShader = [](ShaderHandle shader, auto& psoShader){
            if(shader != nullptr){
                const void* byteCode{};
                size_t byteSize{};
                shader->GetBytecode(&byteCode, &byteSize);
                psoShader = { byteCode, byteSize };
            }
        };
        setShader(desc.AS, psoStream.AmplificationShader);
        setShader(desc.MS, psoStream.MeshShader);
        setShader(desc.PS, psoStream.PixelShader);
        
        psoStream.RasterizerState = ConvertRasterizerState(desc.renderState.rasterState);
        psoStream.BlendState = ConvertBlendState(desc.renderState.blendState);
        
        const auto& dsState = desc.renderState.depthStencilState;
        psoStream.DepthStencilState = ConvertDepthStencilState(dsState);
        if((dsState.depthTestEnable || dsState.stencilEnable) && fbInfo.depthFormat == Format::UNKNOWN){
            psoStream.DepthStencilState.DepthEnable = false;
            psoStream.DepthStencilState.StencilEnable = false;
        }

        psoStream.SampleDesc = { fbInfo.sampleCount, fbInfo.sampleQuality };
        psoStream.SampleMask = ~0u;

        for(int i = 0; i < fbInfo.colorFormats.size(); ++i){
            psoStream.RenderTargets.RTFormats[i] = GetDxgiFormatMapping(fbInfo.colorFormats[i]).rtvFormat;
        }
        psoStream.RenderTargets.NumRenderTargets = fbInfo.colorFormats.size();
        psoStream.DSVFormat = GetDxgiFormatMapping(fbInfo.depthFormat).rtvFormat;

        D3D12_PIPELINE_STATE_STREAM_DESC psoDesc{};
        psoDesc.pPipelineStateSubobjectStream = &psoStream;
        psoDesc.SizeInBytes = sizeof(psoStream);

        RefPtr<ID3D12PipelineState> pipelineState;
        auto hr = m_Context.m_Device2->CreatePipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
        if(FAILED(hr)){
            m_Context.Error("Failed to create a meshlet pipeline state object");
            return nullptr;
        }

        return CreateHandleForNativeMeshletPipeline(rootSig.Get(), pipelineState.Get(), desc, fbInfo);
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

    void Device::ResizeDescriptorTable(IDescriptorTable * _descriptorTable, uint32_t newSize, bool keepContents)
    {
        DescriptorTable* descriptorTable = Utility::CheckedCast<DescriptorTable*>(_descriptorTable);
        if(descriptorTable == nullptr || newSize == descriptorTable->GetCapacity()) return;

        auto preCapacity = descriptorTable->GetCapacity();
        auto preBaseIndex = descriptorTable->GetFirstDescriptorIndex();
        // 需要缩小
        if(newSize < preCapacity){
            m_Resources.shaderResourceViewHeap.ReleaseDescriptors(preBaseIndex, preCapacity - newSize);
            descriptorTable->capacity = newSize;
            return;
        }

        // 需要扩大
        descriptorTable->firstDescriptor = m_Resources.shaderResourceViewHeap.AllocateDescriptors(newSize);
        descriptorTable->capacity = newSize;
        if(preCapacity > 0){
            if(keepContents){   // 拷贝旧资源
                m_Context.m_Device->CopyDescriptorsSimple(
                    preCapacity, 
                    m_Resources.shaderResourceViewHeap.GetCpuHandle(descriptorTable->firstDescriptor),
                    m_Resources.shaderResourceViewHeap.GetCpuHandle(preBaseIndex),
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                m_Context.m_Device->CopyDescriptorsSimple(
                    preCapacity, 
                    m_Resources.shaderResourceViewHeap.GetCpuHandleShaderVisible(descriptorTable->firstDescriptor),
                    m_Resources.shaderResourceViewHeap.GetCpuHandle(preBaseIndex),
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }
            // 释放旧资源
            m_Resources.shaderResourceViewHeap.ReleaseDescriptors(preBaseIndex, preCapacity);
        }
    }

    bool Device::WriteDescriptorTable(IDescriptorTable *_descriptorTable, const BindingSetItem &item)
    {
        auto descriptorTable = Utility::CheckedCast<DescriptorTable*>(_descriptorTable);
        if (descriptorTable == nullptr || item.resourceHandle == nullptr ||
            item.slot > descriptorTable->GetCapacity()) return false;

        uint32_t descriptorIndex = descriptorTable->GetFirstDescriptorIndex() + item.slot;
        auto cpuHandle = m_Resources.shaderResourceViewHeap.GetCpuHandle(descriptorIndex);

        switch (item.type) {
        case ResourceType::None:{
            Buffer::CreateNullSRV(cpuHandle.ptr, item.format, m_Context);
            break;
        }
        case ResourceType::Texture_SRV:{
            Texture* tex = Utility::CheckedCast<Texture*>(item.resourceHandle);
            tex->CreateSRV(cpuHandle.ptr, item.format, item.dimension, item.subresources);
            break;
        }
        case ResourceType::Texture_UAV:{
            Texture* tex = Utility::CheckedCast<Texture*>(item.resourceHandle);
            tex->CreateUAV(cpuHandle.ptr, item.format, item.dimension, item.subresources);
            break;
        }
        case ResourceType::TypedBuffer_SRV:
        case ResourceType::RawBuffer_SRV:
        case ResourceType::StructuredBuffer_SRV:{
            Buffer* buffer = Utility::CheckedCast<Buffer*>(item.resourceHandle);
            buffer->CreateSRV(cpuHandle.ptr, item.format, item.range, item.type);
            break;
        }
        case ResourceType::TypedBuffer_UAV:
        case ResourceType::RawBuffer_UAV:
        case ResourceType::StructuredBuffer_UAV:{
            Buffer* buffer = Utility::CheckedCast<Buffer*>(item.resourceHandle);
            buffer->CreateUAV(cpuHandle.ptr, item.format, item.range, item.type);
            break;
        }
        case ResourceType::ConstantBuffer:{
            Buffer* buffer = Utility::CheckedCast<Buffer*>(item.resourceHandle);
            buffer->CreateCBV(cpuHandle.ptr, item.range);
            break;
        }
        case ResourceType::RayTracingAccelStruct:{
            // TODO: 随后支持光追时实现对应逻辑
            break;
        }
        case ResourceType::VolatileConstantBuffer:{
            m_Context.Error("Attempted to bind a volatile constant buffer to a bindless set.");
            return false;
        }
        default:
            assert(!"Invalid resource type on write descriptor table.");
            return false;
        }

        m_Resources.shaderResourceViewHeap.CopyToShaderVisibleHeap(descriptorIndex);
        return true;
    }

    IMessageCallback *Device::GetMessageCallback()
    {
        return m_Context.m_MessageCallback;
    }

    GraphicsPipelineHandle Device::CreateHandleForNativeGraphicsPipeline(
        IRootSignature *rootSignature, 
        ID3D12PipelineState *pipelineState, 
        const GraphicsPipelineDesc &desc, 
        const FramebufferInfo &framebufferInfo)
    {
        if(rootSignature == nullptr || pipelineState == nullptr)
            return GraphicsPipelineHandle(nullptr);

        GraphicsPipeline* pso = new GraphicsPipeline(desc, framebufferInfo);
        pso->rootSignature = Utility::CheckedCast<RootSignature*>(rootSignature);
        pso->pipelineState = pipelineState;
        pso->requiresBlendFactor = desc.renderState.blendState.UsesConstantColor(framebufferInfo.colorFormats.size());

        return GraphicsPipelineHandle{pso};
    }

    MeshletPipelineHandle Device::CreateHandleForNativeMeshletPipeline(
        IRootSignature *rootSignature, 
        ID3D12PipelineState *pipelineState, 
        const MeshletPipelineDesc &desc, 
        const FramebufferInfo &framebufferInfo)
    {
        if(rootSignature == nullptr || pipelineState == nullptr)
            return nullptr;

        MeshletPipeline* pso = new MeshletPipeline(desc, framebufferInfo);
        pso->rootSignature = Utility::CheckedCast<RootSignature*>(rootSignature);
        pso->pipelineState = pipelineState;
        pso->requiresBlendFactor = desc.renderState.blendState.UsesConstantColor(framebufferInfo.colorFormats.size());

        return MeshletPipelineHandle{pso};
    }

    IDescriptorHeap *Device::GetDescriptorHeap(DescriptorHeapType heapType)
    {
        switch (heapType) {
        case DescriptorHeapType::ShaderResourceView:
            return &m_Resources.shaderResourceViewHeap;
        case DescriptorHeapType::RenderTargetView:
            return &m_Resources.renderTargetViewHeap;
        case DescriptorHeapType::DepthStencilView:
            return &m_Resources.depthStencilViewHeap;
        case DescriptorHeapType::Sampler:
            return &m_Resources.samplerHeap;
        default:
            return nullptr;
        }
    }

    RefPtr<RootSignature> Device::GetRootSignature(const BindingLayoutVector &pipelineLayouts, bool allowInputLayout)
    {
        size_t hash = 0;
        for (const auto &bindingLayout : pipelineLayouts) {
            hash = Utility::HashCombine(hash, bindingLayout);
        }
        hash = Utility::HashCombine(hash, allowInputLayout);

        RefPtr<RootSignature> rootSig = nullptr;
        if(auto it = m_Resources.rootsigCache.find(hash); it != m_Resources.rootsigCache.end()) {
            rootSig = it->second;
        }
        else{
            rootSig = Utility::CheckedCast<RootSignature*>(BuildRootSignature(pipelineLayouts, allowInputLayout, false).Get());
            rootSig->hash = hash;
            m_Resources.rootsigCache[hash] = rootSig.Get();
        }

        return rootSig;
    }

    RootSignatureHandle Device::BuildRootSignature(
        const StaticVector<BindingLayoutHandle, c_MaxBindingLayouts> &pipelineLayouts, 
        bool allowInputLayout, bool isLocal, 
        const D3D12_ROOT_PARAMETER1 *pCustomParameters, 
        uint32_t numCustomParameters)
    {
        assert(pCustomParameters != nullptr && numCustomParameters != 0 || 
            pCustomParameters == nullptr && numCustomParameters == 0);

        RootSignature* rootSig = new RootSignature(m_Resources);
        
        std::vector<D3D12_ROOT_PARAMETER1> rootParameters(numCustomParameters);
        for(uint32_t i = 0; i < numCustomParameters; ++i){
            rootParameters[i] = pCustomParameters[i];
        }

        bool useSamplersHeap = false;
        bool useSRVsHeap = false;

        for(const auto& layout : pipelineLayouts){
            uint32_t rootParameterOffset = rootParameters.size();
            // 普通根参数
            if(layout->GetDesc() != nullptr){
                BindingLayout* bindingLayout = Utility::CheckedCast<BindingLayout*>(layout.Get());

                rootSig->pipelineLayouts.emplace_back(rootParameterOffset, bindingLayout);
                rootParameters.append_range(bindingLayout->rootParameters);

                if(bindingLayout->pushConstantByteSize > 0){
                    rootSig->pushConstantByteSize = bindingLayout->pushConstantByteSize;
                    rootSig->rootConstantsIndex = rootParameterOffset + bindingLayout->rootConstantsIndex;
                }
            }
            else if(layout->GetBindlessDesc() != nullptr){  // Bindless 资源
                BindlessLayout* bindlessLayout = Utility::CheckedCast<BindlessLayout*>(layout.Get());

                auto layoutType = bindlessLayout->GetBindlessDesc()->layoutType;
                if(layoutType == BindlessLayoutDesc::LayoutType::Immutable){
                    rootSig->pipelineLayouts.emplace_back(rootParameterOffset, bindlessLayout);
                    rootParameters.push_back(bindlessLayout->rootParameter);
                }
                else{
                    rootSig->pipelineLayouts.emplace_back(c_InvalidRootParameterIndex, bindlessLayout);
                    useSamplersHeap = layoutType == BindlessLayoutDesc::LayoutType::MutableSampler;
                    useSRVsHeap = !useSamplersHeap;
                }
            }
            else{
                m_Context.Error("Invalid binding layout in root signature.");
                delete rootSig;
                return RootSignatureHandle{nullptr};
            }
        }

        // 根签名描述
        D3D12_ROOT_SIGNATURE_DESC1 rsDesc1{};
        if(!rootParameters.empty()){    
            rsDesc1.NumParameters = static_cast<UINT>(rootParameters.size());
            rsDesc1.pParameters = rootParameters.data();
        }
        if(allowInputLayout){
            rsDesc1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        }
        if(isLocal){
            rsDesc1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        }
        if(m_HeapDirectlyIndexedEnabled){
            if(useSamplersHeap){
                rsDesc1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
            }
            if(useSRVsHeap){
                rsDesc1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
            }
        }

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rsDesc.Desc_1_1 = rsDesc1;

        RefPtr<ID3DBlob> signature{};
        RefPtr<ID3DBlob> error{};
        auto hr = D3D12SerializeVersionedRootSignature(&rsDesc, signature.GetAddressOf(), error.GetAddressOf());
        if(FAILED(hr)){
            std::string msg = std::format("Failed to serialize root signature,Error msg: {}.", GetErrorMessage(hr));
            if(error != nullptr && error->GetBufferSize() > 0){
                msg += std::string(static_cast<const char*>(error->GetBufferPointer())) + ".";
            }
            m_Context.Error(msg);
            delete rootSig;
            return RootSignatureHandle{nullptr};
        }

        hr = m_Context.m_Device->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(rootSig->rootSignature.GetAddressOf()));
        
        if(FAILED(hr)){
            std::string msg = std::format("Failed to create root signature, Error msg: {}", GetErrorMessage(hr));
            m_Context.Error(msg);
            delete rootSig;
            return RootSignatureHandle{nullptr};
        }
        
        return RootSignatureHandle(rootSig);
    }


}

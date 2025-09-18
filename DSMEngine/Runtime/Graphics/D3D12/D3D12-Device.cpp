#include "D3D12-Device.h"
#include "D3D12-Texture.h"
#include "D3D12-Heap.h"
#include "D3D12-Buffer.h"
#include "D3D12-Sampler.h"
#include "D3d12-FrameBuffer.h"
#include "D3D12-ResourceBindings.h"
#include "D3D12-Shader.h"
#include "D3D12-PipelineState.h"
#include "D3D12-CommandList.h"
#include <format>
#include <dxgi1_6.h>
#include <dxgidebug.h>

// 通过栅栏值的偏移来直接获取队列的类型
#define QUEUE_TYPE_MOVEBITS 56

namespace DSM::D3D12{
    DXGI_FORMAT ConvertFormat(DSM::Format format)
    {
        return GetDxgiFormatMapping(format).srvFormat;
    }

    DeviceHandle CreateDevice(const DeviceDesc& desc)
    {
        return DeviceHandle{new Device(desc)};
    }


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
            if(FAILED(m_Context.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &info, sizeof(info)))){
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
        m_NextFenceValue(((uint64_t)queueType << QUEUE_TYPE_MOVEBITS) | 1)
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
            return;
        }
        const auto& context = m_Device.GetContext();
        auto hr = context.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_CommandQueue.GetAddressOf()));
        if(FAILED(hr)){
            context.Error(std::format("Failed to create command queue. Error msg: {}.", Utility::GetHRErrorMessage(hr)));
            m_CommandQueue = nullptr;
            return;
        }

        hr = context.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_Fence.GetAddressOf()));
        if(FAILED(hr)){
            context.Error(std::format("Failed to create fence. Error msg: {}.", Utility::GetHRErrorMessage(hr)));
            m_CommandQueue = nullptr;
            return;
        }
        m_Fence->SetName(L"CommandQueue::m_Fence");
        m_Fence->Signal(m_LastCompletedFenceValue);

        m_FenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
        if(m_FenceEventHandle == nullptr){
            m_CommandQueue = nullptr;
        }
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

    uint64_t CommandQueue::ExecuteCommandList(std::span<DSM::ICommandList* const> cmdLists)
    {
        assert(!cmdLists.empty());
    
        std::lock_guard<std::mutex> guard{m_EventMutex};

        std::vector<ID3D12CommandList*> d3dCmdLists{};
        d3dCmdLists.reserve(cmdLists.size());
        for(const auto& cmdList : cmdLists){
            d3dCmdLists.push_back(Utility::CheckedCast<CommandList*>(cmdList)->GetNativeObject(ObjectTypes::D3D12_GraphicsCommandList));
        }

        m_CommandQueue->ExecuteCommandLists(uint32_t(d3dCmdLists.size()), d3dCmdLists.data());
        auto fenceValue = IncrementFence();

        for (const auto& cmdList : cmdLists) {
            // 执行完后 cmdList 内的命令列表变为空
            auto instance = Utility::CheckedCast<CommandList*>(cmdList)->Executed(*this);
            m_ActiveCmdLists.push(instance);
        }
        m_RecordingInstance++;

        return fenceValue;
    }

    void CommandQueue::ClearCompletedCmdList()
    {
        while (!m_ActiveCmdLists.empty() && 
            IsFenceComplete(m_ActiveCmdLists.front()->submitFenceValue)) {
            m_ActiveCmdLists.pop();
        }
    }





    //////////////////////////////////////////////////////////////////////////
    // Device
    //////////////////////////////////////////////////////////////////////////

    Device::Device(DeviceDesc desc)
        :m_Desc(std::move(desc)), m_Resources(m_Context, m_Desc) {
        m_Context.messageCallback = desc.errorCB;
        m_Context.logBufferLifetime = desc.logBufferLifetime;
        m_Context.stateTracker = std::make_unique<ResourceStateTracker>(m_Context.messageCallback);

        DWORD factoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG) && 0
        // 开启调试层
        RefPtr<ID3D12Debug> pDebug{};
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(pDebug.GetAddressOf())))) {
            pDebug->EnableDebugLayer();
            RefPtr<ID3D12Debug1> pDebug1{};
            if (SUCCEEDED(pDebug->QueryInterface(IID_PPV_ARGS(pDebug1.GetAddressOf())))) {
                pDebug1->SetEnableGPUBasedValidation(true);
            }
        }
        else {
            m_Context.messageCallback->Message(MessageSeverity::Warning, "Failed to get D3D12 debug interface");
        }

        RefPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf())))) {
            factoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] = {
                80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
            };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
        }
#endif
        auto errorMsg = [this](const std::string& msg, HRESULT hr){
            std::string error = std::format("{}.Error msg: {}.", msg, Utility::GetHRErrorMessage(hr));
            m_Context.Error(error);
            throw std::runtime_error(error);
        };

        RefPtr<IDXGIFactory6> dxgiFactory;
        auto hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
        if(FAILED(hr)){
            errorMsg("Create dxgifactory failed.", hr);
        }

        // 创建图形设备
        RefPtr<IDXGIAdapter1> dxgiAdapter;
        RefPtr<ID3D12Device> device;
        SIZE_T maxSize{};
        for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(i, &dxgiAdapter); ++i) {
            DXGI_ADAPTER_DESC1 dxgiDesc{};
            dxgiAdapter->GetDesc1(&dxgiDesc);

            if (HasFlags((DXGI_ADAPTER_FLAG)dxgiDesc.Flags , DXGI_ADAPTER_FLAG_SOFTWARE) ||
                dxgiDesc.DedicatedVideoMemory < maxSize) {
                continue;
            }
            if(hr = D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)); 
                FAILED(hr)) { continue; }

            maxSize = dxgiDesc.DedicatedVideoMemory;

            m_Context.Info(std::format("Selected GPU:  {} ({} MB)", 
                Utility::WStringToUTF8(dxgiDesc.Description), dxgiDesc.DedicatedVideoMemory >> 20));
        }
        
        // 硬件不支持则使用软适配器
        if (device == nullptr) {
            m_Context.Info("Failed to find a hardware adapter.  Falling back to WARP.\n");
            hr = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(dxgiAdapter.GetAddressOf()));
            hr = D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()));
            if(FAILED(hr)){
                errorMsg("Failed to create device.", hr);
            }
        }

        
        // 创建设备相关资源
        m_Context.device = device;
        for(size_t i = 0; i < size_t(CommandQueueType::Count); ++i){
            m_CommandQueues[i] = std::make_unique<CommandQueue>(*this, CommandQueueType(i));
            if(m_CommandQueues[i]->GetCommandQueue() == nullptr){   // 创建队列失败则置空
                m_CommandQueues[i] = nullptr;
            }
        }
        if(m_CommandQueues[size_t(CommandQueueType::Graphics)] == nullptr){
            errorMsg("Failed to create graphics command queue.", 0);
        }
        
        m_Resources.shaderResourceViewHeap.AllocateResource(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_Desc.shaderResourceViewHeapSize, true);
        m_Resources.depthStencilViewHeap.AllocateResource(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV, m_Desc.depthStencilViewHeapSize, false);
        m_Resources.renderTargetViewHeap.AllocateResource(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_Desc.renderTargetViewHeapSize, false);
        m_Resources.samplerHeap.AllocateResource(
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, m_Desc.samplerHeapSize, true);
        

        // 检测特性支持
        m_Context.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &m_Options, sizeof(m_Options));
        m_Context.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &m_Options1, sizeof(m_Options1));
        bool hasOptions5 = SUCCEEDED(m_Context.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &m_Options5, sizeof(m_Options5)));
        bool hasOptions6 = SUCCEEDED(m_Context.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &m_Options6, sizeof(m_Options6)));
        bool hasOptions7 = SUCCEEDED(m_Context.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &m_Options7, sizeof(m_Options7)));

        if (SUCCEEDED(m_Context.device->QueryInterface(&m_Context.device5)) && hasOptions5) {
            m_RayTracingSupported = m_Options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
            m_TraceRayInlineSupported = m_Options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1;
        }
        if (SUCCEEDED(m_Context.device->QueryInterface(&m_Context.device2)) && hasOptions7) {
            m_MeshletsSupported = m_Options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
        }
        if (SUCCEEDED(m_Context.device->QueryInterface(&m_Context.device8)) && hasOptions7) {
            m_SamplerFeedbackSupported = m_Options7.SamplerFeedbackTier >= D3D12_SAMPLER_FEEDBACK_TIER_0_9;
        }        
        if (hasOptions6) {
            m_VariableRateShadingSupported = m_Options6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_2;
        }

        if (desc.enableHeapDirectlyIndexed) {
            D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_6 };
            bool hasShaderModel = SUCCEEDED(m_Context.device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));

            m_HeapDirectlyIndexedEnabled = m_Options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3 && 
                hasShaderModel && shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6;
        }

        // 创建命令签名
        D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
        D3D12_COMMAND_SIGNATURE_DESC csDesc = {};
        csDesc.NumArgumentDescs = 1;
        csDesc.pArgumentDescs = &argDesc;

        csDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
        argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        m_Context.device->CreateCommandSignature(&csDesc, nullptr, IID_PPV_ARGS(&m_Context.drawIndirectSignature));

        csDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
        argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        m_Context.device->CreateCommandSignature(&csDesc, nullptr, IID_PPV_ARGS(&m_Context.drawIndexedIndirectSignature));

        csDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
        argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        m_Context.device->CreateCommandSignature(&csDesc, nullptr, IID_PPV_ARGS(&m_Context.dispatchIndirectSignature));

        m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);
    }

    Device::~Device()
    {
        WaitForIdle();
        if (m_FenceEvent) {
            CloseHandle(m_FenceEvent);
            m_FenceEvent = nullptr;
        }
    }

    Object Device::GetNativeObject(ObjectType type)
    {
        switch (type) {
        case ObjectTypes::D3D12_Device:
            return Object(m_Context.device);
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
        auto hr = m_Context.device->CreateHeap(&heapDesc, IID_PPV_ARGS(d3dHeap.GetAddressOf()));

        if(FAILED(hr)){
            std::string msg = std::format("Failed to create heap {}, error msg: {}", 
                DebugNameToString(d.debugName), Utility::GetHRErrorMessage(hr));
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
        Texture* texture = new Texture(m_Context, m_Resources);
        if(!texture->Create(desc)){
            return nullptr;
        }

        return TextureHandle{texture};
    }
    
    MemoryRequirements Device::GetTextureMemoryRequirements(ITexture *texture)
    {
        Texture* tex = Utility::CheckedCast<Texture*>(texture);

        D3D12_RESOURCE_ALLOCATION_INFO info = m_Context.device->GetResourceAllocationInfo(1, 1, &tex->resourceDesc);
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

        D3D12_CLEAR_VALUE clearValue = ConvertClearValue(texture->GetDesc());
        auto hr = m_Context.device->CreatePlacedResource(
            heap->GetHeap(), 
            offset, 
            &texture->resourceDesc, 
            ConvertResourceStates(texture->GetDesc().initialState),
            texture->GetDesc().useClearValue ? &clearValue : nullptr,
            IID_PPV_ARGS(texture->resource.GetAddressOf()));

        if(FAILED(hr)){
            std::string msg = std::format("Failed to create placed texture {}, error msg: {}", 
                DebugNameToString(texture->GetDesc().debugName), Utility::GetHRErrorMessage(hr));
            m_Context.Error(msg);
            return false;
        }

        texture->heap = heap;
        return true;
    }
    
    TextureHandle Device::CreateHandleForNativeTexture(ObjectType objectType, Object texture, const TextureDesc &desc)
    {
        if(texture.pointer == nullptr) return TextureHandle{nullptr};
        if(objectType != ObjectTypes::D3D12_Resource) return TextureHandle{nullptr};
        
        ID3D12Resource* resource = static_cast<ID3D12Resource*>(texture.pointer);

        Texture* tex = new Texture{m_Context, m_Resources};
        tex->Create(desc ,resource);

        return TextureHandle{tex};
    }

    void Device::GetTextureTiling(ITexture *texture, uint32_t *numTiles, PackedMipDesc *desc, TileShape *_tileShape, uint32_t *_subresourceTilingsNum, SubresourceTiling *_subresourceTilings)
    {
        ID3D12Resource* resource = texture->GetNativeObject(ObjectTypes::D3D12_Resource);
        D3D12_PACKED_MIP_INFO packedMipInfo{};
        D3D12_TILE_SHAPE tileShape{};
        D3D12_SUBRESOURCE_TILING subresourceTilings[16];

        m_Context.device->GetResourceTiling(
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

    void Device::UpdateTextureTileMappings(ITexture *_texture, const TextureTilesMapping *tileMappings, uint32_t numTileMappings, CommandQueueType executionQueue)
    {
        CommandQueue* queue = GetQueue(executionQueue);
        Texture* texture = Utility::CheckedCast<Texture*>(_texture);

        D3D12_TILE_SHAPE tileShape;
        D3D12_SUBRESOURCE_TILING subresourceTiling;
        m_Context.device->GetResourceTiling(texture->resource, nullptr, nullptr, &tileShape, nullptr, 0, &subresourceTiling);

        for (size_t i = 0; i < numTileMappings; i++)
        {
            IHeap* iHeap = tileMappings[i].heap;
            ID3D12Heap* heap = iHeap == nullptr ? Utility::CheckedCast<Heap*>(iHeap)->GetHeap() : nullptr;

            uint32_t numRegions = tileMappings[i].numTextureRegions;
            std::vector<D3D12_TILED_RESOURCE_COORDINATE> resourceCoordinates(numRegions);
            std::vector<D3D12_TILE_REGION_SIZE> regionSizes(numRegions);
            std::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags(numRegions, heap ? D3D12_TILE_RANGE_FLAG_NONE : D3D12_TILE_RANGE_FLAG_NULL);
            std::vector<UINT> heapStartOffsets(numRegions);
            std::vector<UINT> rangeTileCounts(numRegions);

            for (uint32_t j = 0; j < numRegions; ++j)
            {
                const TiledTextureCoordinate& tiledTexCoordinate = tileMappings[i].tiledTextureCoordinates[j];
                const TiledTextureRegion& tiledTexRegion = tileMappings[i].tiledTextureRegions[j];

                resourceCoordinates[j].Subresource = tiledTexCoordinate.mipLevel * texture->GetDesc().arraySize + tiledTexCoordinate.arrayLevel;
                resourceCoordinates[j].X = tiledTexCoordinate.x;
                resourceCoordinates[j].Y = tiledTexCoordinate.y;
                resourceCoordinates[j].Z = tiledTexCoordinate.z;

                if (tiledTexRegion.tilesNum > 0) {
                    regionSizes[j].NumTiles = tiledTexRegion.tilesNum;
                    regionSizes[j].UseBox = false;
                }
                else {
                    uint32_t tilesX = (tiledTexRegion.width + (tileShape.WidthInTexels - 1)) / tileShape.WidthInTexels;
                    uint32_t tilesY = (tiledTexRegion.height + (tileShape.HeightInTexels - 1)) / tileShape.HeightInTexels;
                    uint32_t tilesZ = (tiledTexRegion.depth + (tileShape.DepthInTexels - 1)) / tileShape.DepthInTexels;

                    regionSizes[j].Width = tilesX;
                    regionSizes[j].Height = (uint16_t)tilesY;
                    regionSizes[j].Depth = (uint16_t)tilesZ;

                    regionSizes[j].NumTiles = tilesX * tilesY * tilesZ;
                    regionSizes[j].UseBox = true;
                }

                // Offset in tiles
                if (heap)
                    heapStartOffsets[j] = (uint32_t)(tileMappings[i].byteOffsets[j] / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES);

                rangeTileCounts[j] = regionSizes[j].NumTiles;
            }

            queue->GetCommandQueue()->UpdateTileMappings(
                texture->resource, 
                tileMappings[i].numTextureRegions, 
                resourceCoordinates.data(), 
                regionSizes.data(), 
                heap, 
                numRegions, 
                rangeFlags.data(), 
                heap ? heapStartOffsets.data() : nullptr, 
                rangeTileCounts.data(), 
                D3D12_TILE_MAPPING_FLAG_NONE);
        }
    }




    //////////////////////////////////////////////////////////////////////////
    // Buffer
    //////////////////////////////////////////////////////////////////////////
    BufferHandle Device::CreateBuffer(const BufferDesc &d)
    {
        auto buffer = new Buffer(m_Context, m_Resources);
        if(!buffer->Create(d)){
            return nullptr;
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
                DebugNameToString(buffer->GetDesc().debugName), Utility::GetHRErrorMessage(hr));
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

        auto info = m_Context.device->GetResourceAllocationInfo(1, 1, &buffer->resourceDesc);
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

        auto hr = m_Context.device->CreatePlacedResource(
            heap->GetHeap(), offset,
            &buffer->resourceDesc, resourceState, 
            nullptr, IID_PPV_ARGS(buffer->resource.GetAddressOf()));

        if(FAILED(hr)){
            std::string msg = std::format("Failed to create placed buffer {}, error msg: {}.",
                DebugNameToString(buffer->GetDesc().debugName), Utility::GetHRErrorMessage(hr));
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
        Buffer* buffer = new Buffer{m_Context, m_Resources};
        
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
    InputLayoutHandle Device::CreateInputLayout(std::span<const VertexAttributeDesc> attributes, IShader *vertexShader)
    {
        InputLayout* layout = new InputLayout{};
        layout->attributes.resize(attributes.size());

        for(int i = 0; i < attributes.size(); ++i){
            VertexAttributeDesc& attr = layout->attributes[i];

            attr = attributes[i];

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

    EventQueryHandle Device::CreateEventQuery()
    {
        return EventQueryHandle(new EventQuery());
    }

    void Device::SetEventQuery(IEventQuery *_query, CommandQueueType queue)
    {
        EventQuery* query = Utility::CheckedCast<EventQuery*>(_query);
        CommandQueue* pQueue = GetQueue(queue);
        
        query->started = true;
        query->fence = pQueue->GetFence();
        query->fenceCounter = pQueue->GetNextFenceValue() - 1;
        query->resolved = false;
    }

    bool Device::PollEventQuery(IEventQuery *_query)
    {
        EventQuery* query = Utility::CheckedCast<EventQuery*>(_query);

        if (!query->started)
            return false;

        if (query->resolved)
            return true;

        assert(query->fence);
        
        if (query->fence->GetCompletedValue() >= query->fenceCounter) {
            query->resolved = true;
            query->fence = nullptr;
        }

        return query->resolved;
    }

    void Device::WaitEventQuery(IEventQuery *_query)
    {
        EventQuery* query = Utility::CheckedCast<EventQuery*>(_query);

        if (!query->started || query->resolved)
            return;

        assert(query->fence);

        WaitForFence(query->fence, query->fenceCounter, m_FenceEvent);
    }

    void Device::ResetEventQuery(IEventQuery *_query)
    {
        EventQuery* query = Utility::CheckedCast<EventQuery*>(_query);

        query->started = false;
        query->resolved = false;
        query->fence = nullptr;
    }

    TimerQueryHandle Device::CreateTimerQuery()
    {
        if (!m_Context.timerQueryHeap)
        {
            std::lock_guard lockGuard(m_Mutex);

            if (!m_Context.timerQueryHeap)
            {
                D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
                queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
                queryHeapDesc.Count = uint32_t(m_Resources.timerQueries.GetCapacity()) * 2; // Use 2 D3D12 queries per 1 TimerQuery
                m_Context.device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_Context.timerQueryHeap));

                BufferDesc qbDesc;
                qbDesc.byteSize = queryHeapDesc.Count * 8;
                qbDesc.cpuAccess = CpuAccessMode::Read;

                BufferHandle timerQueryBuffer = CreateBuffer(qbDesc);
                m_Context.timerQueryResolveBuffer = Utility::CheckedCast<Buffer*>(timerQueryBuffer.Get());
            }
        }

        int queryIndex = m_Resources.timerQueries.Allocate();

        if (queryIndex < 0)
            return nullptr;
        
        TimerQuery* query = new TimerQuery(m_Resources);
        query->beginQueryIndex = uint32_t(queryIndex) * 2;
        query->endQueryIndex = query->beginQueryIndex + 1;
        query->resolved = false;
        query->time = 0.f;

        return TimerQueryHandle{query};
    }

    bool Device::PollTimerQuery(ITimerQuery *_query)
    {
        TimerQuery* query = Utility::CheckedCast<TimerQuery*>(_query);

        if (!query->started)
            return false;

        if (!query->fence)
            return true;

        if (query->fence->GetCompletedValue() >= query->fenceCounter) {
            query->fence = nullptr;
            return true;
        }

        return false;
    }

    float Device::GetTimerQueryTime(ITimerQuery *_query)
    {
        TimerQuery* query = Utility::CheckedCast<TimerQuery*>(_query);

        if (!query->resolved)
        {
            if (query->fence)
            {
                WaitForFence(query->fence, query->fenceCounter, m_FenceEvent);
                query->fence = nullptr;
            }

            uint64_t frequency;
            GetQueue(CommandQueueType::Graphics)->GetCommandQueue()->GetTimestampFrequency(&frequency);

            D3D12_RANGE bufferReadRange = {
                query->beginQueryIndex * sizeof(uint64_t),
                (query->beginQueryIndex + 2) * sizeof(uint64_t) };
            uint64_t *data;
            const HRESULT res = m_Context.timerQueryResolveBuffer->resource->Map(0, &bufferReadRange, (void**)&data);

            if (FAILED(res)) {
                m_Context.Error("getTimerQueryTime: Map() failed");
                return 0.f;
            }

            query->resolved = true;
            query->time = float(double(data[query->endQueryIndex] - data[query->beginQueryIndex]) / double(frequency));

            m_Context.timerQueryResolveBuffer->resource->Unmap(0, nullptr);
        }

        return query->time;
    }

    void Device::ResetTimerQuery(ITimerQuery *_query)
    {
        TimerQuery* query = Utility::CheckedCast<TimerQuery*>(_query);

        query->started = false;
        query->resolved = false;
        query->time = 0.f;
        query->fence = nullptr;
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
        if (inputLayout != nullptr) {
            psoDesc.InputLayout.NumElements = inputLayout->inputElements.size();
            psoDesc.InputLayout.pInputElementDescs = inputLayout->inputElements.data();
        }

        RefPtr<ID3D12PipelineState> pipelineState{};
        auto hr = m_Context.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
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
        const auto hr = m_Context.device->CreateComputePipelineState(
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
        auto hr = m_Context.device2->CreatePipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
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
                m_Context.device->CopyDescriptorsSimple(
                    preCapacity, 
                    m_Resources.shaderResourceViewHeap.GetCpuHandle(descriptorTable->firstDescriptor),
                    m_Resources.shaderResourceViewHeap.GetCpuHandle(preBaseIndex),
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                m_Context.device->CopyDescriptorsSimple(
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

    DSM::CommandListHandle Device::CreateCommandList(const CommandListParameters &params)
    {
        if(GetQueue(params.queueType) == nullptr) return nullptr;

        return DSM::CommandListHandle{new CommandList(*this, m_Resources, params)};
    }

    uint64_t Device::ExecuteCommandLists(std::span<DSM::ICommandList* const> cmdLists, CommandQueueType executionQueue)
    {
        CommandQueue* queue = GetQueue(executionQueue);
        assert(queue != nullptr);
        auto fenceValue = queue->ExecuteCommandList(cmdLists);

        HRESULT hr = m_Context.device->GetDeviceRemovedReason();
        if (FAILED(hr)) {
            m_Context.Error(std::format("Execute commandlist error. Error msg: {}!", Utility::GetHRErrorMessage(hr)));
        }

        return fenceValue;
    }

    void Device::QueueWaitForCommandList(CommandQueueType waitQueue, CommandQueueType executionQueue, uint64_t instance)
    {
        GetQueue(waitQueue)->StallForFence(instance);
    }

    bool Device::WaitForIdle()
    {
        // 等待所有的队列执行完成
        for (const auto& pQueue : m_CommandQueues) {
            if (pQueue == nullptr) continue;

            pQueue->WaitForIdle();
        }
        return true;
    }

    Object Device::GetNativeQueue(ObjectType objectType, CommandQueueType queue)
    {
        if (objectType != ObjectTypes::D3D12_CommandQueue ||
            queue >= CommandQueueType::Count) return nullptr;

        CommandQueue* pQueue = GetQueue(queue);

        if (!pQueue) return nullptr;

        return Object(pQueue->GetCommandQueue());
    }

    bool Device::QueryFeatureSupport(Feature feature, void *pInfo, size_t infoSize)
    {
        switch (feature) {
        case Feature::DeferredCommandLists:
            return true;
        case Feature::SinglePassStereo:
            return m_SinglePassStereoSupported;
        case Feature::RayTracingAccelStruct:
            return m_RayTracingSupported;
        case Feature::RayTracingPipeline:
            return m_RayTracingSupported;
        case Feature::RayTracingOpacityMicromap:
            return m_OpacityMicromapSupported;
        case Feature::RayTracingClusters:
            return m_RayTracingClustersSupported;
        case Feature::RayQuery:
            return m_TraceRayInlineSupported;
        case Feature::FastGeometryShader:
            return m_FastGeometryShaderSupported;
        case Feature::ShaderExecutionReordering:
            return m_ShaderExecutionReorderingSupported;
        case Feature::Spheres:
            return m_SpheresSupported;
        case Feature::LinearSweptSpheres:
            return m_LinearSweptSpheresSupported;
        case Feature::Meshlets:
            return m_MeshletsSupported;
        case Feature::VariableRateShading:
            return false;
        case Feature::VirtualResources:
            return true;
        case Feature::ComputeQueue:
            return (GetQueue(CommandQueueType::Compute) != nullptr);
        case Feature::CopyQueue:
            return (GetQueue(CommandQueueType::Copy) != nullptr);
        case Feature::ConservativeRasterization:
            return true;
        case Feature::ConstantBufferRanges:
            return true;
        case Feature::HeapDirectlyIndexed:
            return m_HeapDirectlyIndexedEnabled;
        case Feature::SamplerFeedback:
            return m_SamplerFeedbackSupported;
        case Feature::HlslExtensionUAV:
            return m_HlslExtensionsSupported;
        case Feature::WaveLaneCountMinMax:
            return false;
        case Feature::CooperativeVectorInferencing:
            return m_CoopVecInferencingSupported;
        case Feature::CooperativeVectorTraining:
            return m_CoopVecTrainingSupported;  
        default:
            return false;
        }
    }

    FormatSupport Device::QueryFormatSupport(Format format)
    {
        const DxgiFormatMapping& formatMapping = GetDxgiFormatMapping(format);

        FormatSupport result = FormatSupport::None;

        D3D12_FEATURE_DATA_FORMAT_SUPPORT featureData = {};
        featureData.Format = formatMapping.rtvFormat;

        m_Context.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &featureData, sizeof(featureData));

        if (featureData.Support1 & D3D12_FORMAT_SUPPORT1_BUFFER)
            result = result | FormatSupport::Buffer;
        if (featureData.Support1 & (D3D12_FORMAT_SUPPORT1_TEXTURE1D | D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_TEXTURE3D | D3D12_FORMAT_SUPPORT1_TEXTURECUBE))
            result = result | FormatSupport::Texture;
        if (featureData.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)
            result = result | FormatSupport::DepthStencil;
        if (featureData.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)
            result = result | FormatSupport::RenderTarget;
        if (featureData.Support1 & D3D12_FORMAT_SUPPORT1_BLENDABLE)
            result = result | FormatSupport::Blendable;

        if (formatMapping.srvFormat != featureData.Format)
        {
            featureData.Format = formatMapping.srvFormat;
            featureData.Support1 = (D3D12_FORMAT_SUPPORT1)0;
            featureData.Support2 = (D3D12_FORMAT_SUPPORT2)0;
            m_Context.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &featureData, sizeof(featureData));
        }

        if (featureData.Support1 & D3D12_FORMAT_SUPPORT1_IA_INDEX_BUFFER)
            result = result | FormatSupport::IndexBuffer;
        if (featureData.Support1 & D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER)
            result = result | FormatSupport::VertexBuffer;
        if (featureData.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD)
            result = result | FormatSupport::ShaderLoad;
        if (featureData.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)
            result = result | FormatSupport::ShaderSample;
        if (featureData.Support2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD)
            result = result | FormatSupport::ShaderAtomic;
        if (featureData.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD)
            result = result | FormatSupport::ShaderUavLoad;
        if (featureData.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)
            result = result | FormatSupport::ShaderUavStore;

        return result;
    }

    void Device::RunGarbageCollection()
    {
        for(uint32_t i = 0; i < (uint32_t)CommandQueueType::Count; ++i){
            auto queue = GetQueue((CommandQueueType)i);
            queue->ClearCompletedCmdList();
        }
    }

    IMessageCallback *Device::GetMessageCallback()
    {
        return m_Context.messageCallback;
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

    DescriptorHeapHandle Device::CreateDescriptorHeap(DescriptorHeapType type, uint32_t count, bool shaderVisible)
    {
        auto descriptorHeap = new DescriptorHeap(m_Context);
        switch(type){
        case DescriptorHeapType::ShaderResourceView:
            descriptorHeap->AllocateResource(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, count, shaderVisible);
            break;
        case DescriptorHeapType::RenderTargetView:
            descriptorHeap->AllocateResource(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, count, shaderVisible);
            break;
        case DescriptorHeapType::DepthStencilView:
            descriptorHeap->AllocateResource(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, count, shaderVisible);
            break;
        case DescriptorHeapType::Sampler:
            descriptorHeap->AllocateResource(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, count, shaderVisible);
            break;
        default:
            assert(!"Invalid descriptor heap type.");
            break;    
        }
        return DescriptorHeapHandle(descriptorHeap);
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
        
        // 额外的根参数
        std::vector<D3D12_ROOT_PARAMETER1> rootParameters(numCustomParameters);
        for(uint32_t i = 0; i < numCustomParameters; ++i){
            rootParameters[i] = pCustomParameters[i];
        }

        bool useSamplersHeap = false;
        bool useSRVsHeap = false;

        // 处理每一个绑定布局
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
                if(layoutType == BindlessLayoutDesc::LayoutType::Immutable){    // 使用描述符表
                    rootSig->pipelineLayouts.emplace_back(rootParameterOffset, bindlessLayout);
                    rootParameters.push_back(bindlessLayout->rootParameter);
                }
                else{   // 使用直接索引
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
            std::string msg = std::format("Failed to serialize root signature,Error msg: {}.", Utility::GetHRErrorMessage(hr));
            if(error != nullptr && error->GetBufferSize() > 0){
                msg += std::string(static_cast<const char*>(error->GetBufferPointer())) + ".";
            }
            m_Context.Error(msg);
            delete rootSig;
            return RootSignatureHandle{nullptr};
        }

        hr = m_Context.device->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(rootSig->rootSignature.GetAddressOf()));
        
        if(FAILED(hr)){
            std::string msg = std::format("Failed to create root signature, Error msg: {}", Utility::GetHRErrorMessage(hr));
            m_Context.Error(msg);
            delete rootSig;
            return RootSignatureHandle{nullptr};
        }
        
        return RootSignatureHandle(rootSig);
    }


}

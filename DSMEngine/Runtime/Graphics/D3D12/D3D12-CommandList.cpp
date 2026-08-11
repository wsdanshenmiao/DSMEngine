#include "D3D12-CommandList.h"
#include "D3D12-Texture.h"
#include "D3D12-Buffer.h"
#include "D3D12-Device.h"
#include "D3D12-PipelineState.h"
#include "D3D12-RayTracing.h"
#include "D3D12-ResourceBindings.h"
#include "D3d12-FrameBuffer.h"
#include <functional>
#include <cstring>
#include <ranges>
#include <pix.h>

namespace DSM::D3D12{
    InternalCommandList *InternalCommandList::RequireCommandList(Device &device, const CommandListParameters &desc)
    {        
        auto queue = device.GetQueue(desc.queueType);
        const Context& context = device.GetContext();
        assert(queue != nullptr);

        std::lock_guard lock{sm_Mutex};

        InternalCommandList* cmdList = nullptr;
        auto& retiredQueue = sm_RetiredCmdLists[(size_t)desc.queueType];
        auto& availedQueue = sm_AvailableCmdLists[(size_t)desc.queueType];

        // 弹出可用的命令列表
        while (!retiredQueue.empty() && queue->IsFenceComplete(retiredQueue.front().first)) {
            availedQueue.push(retiredQueue.front().second);
            retiredQueue.pop();
        }

        if(availedQueue.empty()){
            // 创建新的 Command list
            try {
                cmdList = new InternalCommandList(device, desc);
            }
            catch(const std::exception& e) {
                context.Error(e.what());
                return nullptr;
            }
            sm_CmdListPool.emplace(cmdList);
        }
        else{   // 从池中获取
            cmdList = availedQueue.front();
            auto hr = cmdList->allocator->Reset();
            if(FAILED(hr)){
                context.Error(std::format("Failed to reset allocator. Error msg: {}", GetHRErrorMessage(hr)));
                return nullptr;
            }

            hr = cmdList->cmdList->Reset(cmdList->allocator, nullptr);
            if(FAILED(hr)){
                context.Error(std::format("Failed to reset cmdList. Error msg: {}", GetHRErrorMessage(hr)));
                return nullptr;
            }
            cmdList->lastSubmittedFenceValue = 0;
            availedQueue.pop();
        }
        if (!desc.debugName.empty()) {
            auto name = Utility::UTF8ToWString(desc.debugName);
            cmdList->cmdList->SetName(name.c_str());
            name += L"::Allocator";
            cmdList->allocator->SetName(name.c_str());
        }

        return cmdList;
    }

    bool InternalCommandList::ReleaseCommandList(InternalCommandList *cmdList)
    {
        assert(cmdList != nullptr);

        std::lock_guard lock{sm_Mutex};

        auto it = std::find_if(sm_CmdListPool.begin(), sm_CmdListPool.end(), 
            [cmdList](const auto& list) {  return list.get() == cmdList; });
        if(it == sm_CmdListPool.end()) return false;

        auto& retiredQueue = sm_RetiredCmdLists[(size_t)cmdList->type];
        cmdList->uploadBufferAllocator->Cleanup(cmdList->lastSubmittedFenceValue);
        cmdList->gpuBufferAllocator->Cleanup(cmdList->lastSubmittedFenceValue);
        retiredQueue.emplace(cmdList->lastSubmittedFenceValue, cmdList);
        
        return true;
    }

    void InternalCommandList::Cleanup()
    {
        std::lock_guard lock{sm_Mutex};

        for(int i = 0; i < (int)CommandQueueType::Count; ++i){
            while (!sm_AvailableCmdLists[i].empty()){
                sm_AvailableCmdLists[i].pop();
            }
            while (!sm_RetiredCmdLists[i].empty()){
                sm_RetiredCmdLists[i].pop();
            }
        }
        sm_CmdListPool.clear();
    }
    
    InternalCommandList::InternalCommandList(Device& device, const CommandListParameters &desc)
        :type(desc.queueType) {
        D3D12_COMMAND_LIST_TYPE listType{};
        switch (desc.queueType) {
        case CommandQueueType::Graphics:
            listType = D3D12_COMMAND_LIST_TYPE_DIRECT;
            break;
        case CommandQueueType::Compute:
            listType = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            break;
        case CommandQueueType::Copy:
            listType = D3D12_COMMAND_LIST_TYPE_COPY;
            break;
        default:
            throw std::invalid_argument("Invalid command queue type.");
        }

        const Context& context = device.GetContext();

        uploadBufferAllocator = std::make_unique<DynamicResourceAllocator>(
            context, 
            device.GetQueue(desc.queueType), 
            DynamicResourceAllocator::AllocateMode::CpuExclusive, 
            desc.uploadChunkSize);
        gpuBufferAllocator = std::make_unique<DynamicResourceAllocator>(
            context,
            device.GetQueue(desc.queueType), 
            DynamicResourceAllocator::AllocateMode::GpuExclusive,
            desc.scratchChunkSize);

        auto errorMsg = [this](const std::string& msg, auto hr){
            uploadBufferAllocator = nullptr;
            gpuBufferAllocator = nullptr;
            throw std::runtime_error{std::format("{} Error msg: {}", msg, GetHRErrorMessage(hr))};
        };

        auto hr = context.device->CreateCommandAllocator(listType, IID_PPV_ARGS(allocator.GetAddressOf()));
        if(FAILED(hr)){
            errorMsg("Faile to create command allocator.", hr);
        }

        hr = context.device->CreateCommandList(0, listType, allocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf()));
        if(FAILED(hr)){
            errorMsg("Faile to create command list.", hr);
        }

        cmdList->QueryInterface(IID_PPV_ARGS(cmdList4.GetAddressOf()));
        cmdList->QueryInterface(IID_PPV_ARGS(cmdList6.GetAddressOf()));
    }



    CommandList::CommandList(Device& device, std::shared_ptr<DeviceResources> resources, CommandListParameters desc)
        :m_Device(device), 
        m_Resources(resources), 
        m_Desc(std::move(desc)),
        m_StateTracker(*device.GetContext().stateTracker) { }

    Object CommandList::GetNativeObject(ObjectType type)
    {
        switch (type) {
        case ObjectTypes::D3D12_GraphicsCommandList:
            if(m_CurrCmdList != nullptr) return Object{m_CurrCmdList->cmdList.Get()};
            break;
        case ObjectTypes::D3D12_CommandAllocator:
            if(m_CurrCmdList != nullptr) return Object{m_CurrCmdList->allocator.Get()};
            break;
        default:
            return Object{nullptr};
        }
        return Object{nullptr};
    }

    void CommandList::Open()
    {
        // 在命令列表提交时释放
        m_CurrCmdList = InternalCommandList::RequireCommandList(m_Device, m_Desc);
        assert(m_CurrCmdList != nullptr);

        m_Instance = std::make_shared<CommandListInstance>();
        m_Instance->queueType = m_Desc.queueType;
        m_Instance->allocator = m_CurrCmdList->allocator;
        m_Instance->cmdList = m_CurrCmdList->cmdList;
    }

    void CommandList::Close()
    {
        m_StateTracker.KeepBufferInitialStates();
        m_StateTracker.KeepTextureInitialStates();
        CommitBarriers();

        auto hr = m_CurrCmdList->cmdList->Close();
        if(FAILED(hr)){
            std::string msg = std::format("Failed to close command list. Error msg: {}", GetHRErrorMessage(hr));
            m_Device.GetContext().Error(msg);
            return;
        }

        ClearStateCache();

        m_VolatileBufferAddresses.clear();
    }

    void CommandList::ClearState()
    {
        m_CurrCmdList->cmdList->ClearState(nullptr);
        ClearStateCache();
        CommitDescriptorHeaps();
    }

    void CommandList::ClearTextureFloat(ITexture *t, TextureSubresourceSet subresources, const Color &clearColor)
    {
        Texture* texture = Utility::CheckedCast<Texture*>(t);
        
#ifdef _DEBUG
        const FormatInfo& formatInfo = GetFormatInfo(t->GetDesc().format);
        assert(!formatInfo.hasDepth && !formatInfo.hasStencil);
        assert(t->GetDesc().isUAV || t->GetDesc().isRenderTarget);
#endif

        const TextureDesc& desc = texture->GetDesc();

        subresources = subresources.Resolve(desc, false);
        m_Instance->refResources.push_back(t);

        if(desc.isRenderTarget){
            if(m_EnableAutomaticBarriers){
                m_StateTracker.RequireTextureState(texture, subresources, ResourceStates::RenderTarget);
            }
            CommitBarriers();

            Object nativeView = texture->GetNativeView(ObjectTypes::D3D12_RenderTargetViewDescriptor, Format::UNKNOWN, subresources);
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = { nativeView.integer };
            m_CurrCmdList->cmdList->ClearRenderTargetView(rtv, &clearColor.r, 0, nullptr);
        }
        else if(desc.isUAV) {
            if(m_EnableAutomaticBarriers){
                m_StateTracker.RequireTextureState(texture, subresources, ResourceStates::UnorderedAccess);
            }
            CommitBarriers();
            // 更新描述符堆
            CommitDescriptorHeaps();

            for(uint32_t i = 0; i < subresources.numMipLevels; ++i){
                uint32_t mipLevel = subresources.baseArraySlice + i;
                uint32_t descriptorIndex = texture->GetClearMipLevelUAV(mipLevel);
                
                if(auto resources = m_Resources.lock()) {
                    m_CurrCmdList->cmdList->ClearUnorderedAccessViewFloat(
                        resources->shaderResourceViewHeap.GetGpuHandle(descriptorIndex),
                        resources->shaderResourceViewHeap.GetCpuHandle(descriptorIndex),
                        texture->resource.Get(), &clearColor.r, 0 ,nullptr);
                }
            }
        }
    }

    void CommandList::ClearTextureUInt(ITexture *t, TextureSubresourceSet subresources, uint32_t clearColor)
    {
        Texture* texture = Utility::CheckedCast<Texture*>(t);
        
#ifdef _DEBUG
        const FormatInfo& formatInfo = GetFormatInfo(t->GetDesc().format);
        assert(!formatInfo.hasDepth && !formatInfo.hasStencil);
        assert(t->GetDesc().isUAV || t->GetDesc().isRenderTarget);
#endif

        const TextureDesc& desc = texture->GetDesc();

        subresources = subresources.Resolve(desc, false);
        m_Instance->refResources.push_back(t);

        uint32_t clearValues[4] = { clearColor, clearColor, clearColor, clearColor };

        if(desc.isRenderTarget){
            if(m_EnableAutomaticBarriers){
                m_StateTracker.RequireTextureState(texture, subresources, ResourceStates::RenderTarget);
            }
            CommitBarriers();

            float clearFloats[4] = { float(clearColor), float(clearColor), float(clearColor), float(clearColor)};
            Object nativeView = texture->GetNativeView(ObjectTypes::D3D12_RenderTargetViewDescriptor, Format::UNKNOWN, subresources);
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = { nativeView.integer };
            m_CurrCmdList->cmdList->ClearRenderTargetView(rtv, clearFloats, 0, nullptr);
        }
        else if(desc.isUAV) {
            if(m_EnableAutomaticBarriers){
                m_StateTracker.RequireTextureState(texture, subresources, ResourceStates::UnorderedAccess);
            }
            CommitBarriers();
            // 更新描述符堆
            CommitDescriptorHeaps();

            for(uint32_t i = 0; i < subresources.numMipLevels; ++i){
                uint32_t mipLevel = subresources.baseArraySlice + i;
                uint32_t descriptorIndex = texture->GetClearMipLevelUAV(mipLevel);
                
                if(auto resources = m_Resources.lock()){
                    m_CurrCmdList->cmdList->ClearUnorderedAccessViewUint(
                        resources->shaderResourceViewHeap.GetGpuHandle(descriptorIndex),
                        resources->shaderResourceViewHeap.GetCpuHandle(descriptorIndex),
                        texture->resource.Get(), clearValues, 0 ,nullptr);
                }
            }
        }
    }

    void CommandList::ClearDepthStencilTexture(ITexture *t, TextureSubresourceSet subresources, 
        bool clearDepth, float depth, bool clearStencil, uint8_t stencil)
    {
        if(!clearDepth && !clearStencil) return;

        const auto& desc = t->GetDesc();
#ifdef _DEBUG
        const FormatInfo& formatInfo = GetFormatInfo(desc.format);
        assert(desc.isRenderTarget);
        assert(formatInfo.hasDepth || formatInfo.hasStencil);
#endif

        subresources = subresources.Resolve(desc, false);
        m_Instance->refResources.push_back(t);

        Texture* tex = Utility::CheckedCast<Texture*>(t);
        D3D12_CLEAR_FLAGS flags{};
        if(clearDepth) {
            flags |= D3D12_CLEAR_FLAG_DEPTH;
        }
        if(clearStencil){
            flags |= D3D12_CLEAR_FLAG_STENCIL;
        }

        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireTextureState(tex, subresources, ResourceStates::DepthWrite);
        }
        CommitBarriers();

        Object nativeView = tex->GetNativeView(ObjectTypes::D3D12_DepthStencilViewDescriptor, Format::UNKNOWN, subresources);
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor = {nativeView.integer};
        m_CurrCmdList->cmdList->ClearDepthStencilView(descriptor, flags, depth, stencil, 0, nullptr);
    }

    void CommandList::CopyTexture(ITexture *_dest, TextureSlice destSlice, ITexture *_src, TextureSlice srcSlice)
    {
        // 添加引用
        m_Instance->refResources.push_back(_dest);
        m_Instance->refResources.push_back(_src);

        const auto& destDesc = _dest->GetDesc();
        const auto& srcDesc =_src->GetDesc();

        destSlice = destSlice.Resolve(destDesc);
        srcSlice = srcSlice.Resolve(srcDesc);

        auto dest = Utility::CheckedCast<Texture*>(_dest);
        auto src = Utility::CheckedCast<Texture*>(_src);

        D3D12_TEXTURE_COPY_LOCATION destLocation{};
        destLocation.pResource = dest->resource;
        destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destLocation.SubresourceIndex = CalculateSubresource(
            destSlice.mipLevel, destSlice.arraySlice, 0, destDesc.mipLevels, destDesc.arraySize);
        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = src->resource;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLocation.SubresourceIndex = CalculateSubresource(
            srcSlice.mipLevel, srcSlice.arraySlice, 0, srcDesc.mipLevels, srcDesc.arraySize);
        
        D3D12_BOX srcBox{};
        srcBox.left = srcSlice.x;
        srcBox.top = srcSlice.y;
        srcBox.front = srcSlice.z;
        srcBox.right = srcSlice.x + srcSlice.width;
        srcBox.bottom = srcSlice.y + srcSlice.height;
        srcBox.back = srcSlice.z + srcSlice.depth;

        // 更改纹理状态
        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireTextureState(
                dest, {destSlice.mipLevel, 1, destSlice.arraySlice, 1}, ResourceStates::CopyDest);
            m_StateTracker.RequireTextureState(
                src, {srcSlice.mipLevel, 1, srcSlice.arraySlice, 1}, ResourceStates::CopySource);        
        }
        CommitBarriers();

        m_CurrCmdList->cmdList->CopyTextureRegion(
            &destLocation, destSlice.x, destSlice.y, destSlice.z, 
            &srcLocation, &srcBox);
    }

    void CommandList::WriteTexture(ITexture *_dest, uint32_t arraySlice, uint32_t mipLevel, 
        const void *data, size_t rowPitch, size_t depthPitch)
    {
        Texture* texture = Utility::CheckedCast<Texture*>(_dest);
        const auto& texDesc = texture->GetDesc();

        uint32_t subresource = CalculateSubresource(
            mipLevel, arraySlice, 0, texDesc.mipLevels, texDesc.arraySize);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        uint32_t numRows;
        uint64_t rowSizeInBytes;
        uint64_t totalBytes;
        m_Device.GetContext().device->GetCopyableFootprints(
            &texture->resourceDesc, subresource, 1, 0, 
            &footprint, &numRows, &rowSizeInBytes, &totalBytes);

        auto uploadBuffer = AllocateUploadBuffer(totalBytes);
        footprint.Offset = uploadBuffer.offset;
        // 每一个 depth
        for(uint32_t depth = 0; depth < footprint.Footprint.Depth; depth++){
            uint64_t destOffset = depth * footprint.Footprint.RowPitch * numRows;
            uint64_t srcOffset = depthPitch * depth;
            // 每一行
            for(uint32_t height = 0; height < numRows; ++height){
                auto destAddress = (uint8_t*)uploadBuffer.mappedAddress;
                destAddress += destOffset + footprint.Footprint.RowPitch * height;
                const void* srcAddress = (const uint8_t*)data + rowPitch * height;
                memcpy(destAddress, srcAddress, std::min(rowSizeInBytes, rowPitch));
            }
        }

        D3D12_TEXTURE_COPY_LOCATION destLocation{};
        destLocation.pResource = texture->resource;
        destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destLocation.SubresourceIndex = subresource;
        D3D12_TEXTURE_COPY_LOCATION srcLocation{};
        srcLocation.pResource = uploadBuffer.resource;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint = footprint;

        m_Instance->refNativeResources.push_back(uploadBuffer.resource);
        m_Instance->refResources.push_back(texture);


        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireTextureState(texture, {mipLevel, 1, arraySlice, 1}, ResourceStates::CopyDest);   
        }
        CommitBarriers();

        m_CurrCmdList->cmdList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
    }

    void CommandList::ResolveTexture(ITexture *_dest, TextureSubresourceSet destSubresources, 
        ITexture *_src, TextureSubresourceSet srcSubresources)
    {
        const auto& destDesc = _dest->GetDesc();
        const auto& srcDesc = _src->GetDesc();

        destSubresources = destSubresources.Resolve(destDesc, false);
        srcSubresources = srcSubresources.Resolve(srcDesc, false);

        Texture* dest = Utility::CheckedCast<Texture*>(_dest);
        Texture* src = Utility::CheckedCast<Texture*>(_src);

        if(destSubresources.numMipLevels != srcSubresources.numMipLevels || 
            destSubresources.numArraySlices != srcSubresources.numArraySlices) return;
        
        // 设置屏障
        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireTextureState(dest, destSubresources, ResourceStates::ResolveDest);
            m_StateTracker.RequireTextureState(src, srcSubresources, ResourceStates::ResolveSource);
        }
        CommitBarriers();

        DXGI_FORMAT format = GetDxgiFormatMapping(destDesc.format).rtvFormat;

        for(uint32_t i = 0; i < destSubresources.numArraySlices; ++i){
            uint32_t destArray = i + destSubresources.baseArraySlice;
            uint32_t srcArray = i + srcSubresources.baseArraySlice;
            for(uint32_t j = 0; j < destSubresources.numMipLevels; ++j){
                uint32_t destMip = j + destSubresources.baseMipLevel;
                uint32_t srcMip = j + srcSubresources.baseMipLevel;
                for(uint8_t plane = 0; plane < dest->planeCount; ++plane){
                    uint32_t destIndex = CalculateSubresource(destMip, destArray, plane, destDesc.mipLevels, destDesc.arraySize);
                    uint32_t srcIndex = CalculateSubresource(srcMip, srcArray, plane, srcDesc.mipLevels, srcDesc.arraySize);
                    m_CurrCmdList->cmdList->ResolveSubresource(dest->resource.Get(), destIndex, src->resource.Get(), srcIndex, format);
                }
            }
        }
    }

    void CommandList::CopyBuffer(IBuffer *_dest, uint64_t destOffsetBytes, 
        IBuffer *_src, uint64_t srcOffsetBytes, uint64_t dataSizeBytes)
    {
        Buffer* dest = Utility::CheckedCast<Buffer*>(_dest);
        Buffer* src = Utility::CheckedCast<Buffer*>(_src);

        m_Instance->refBuffer.push_back(dest);
        m_Instance->refBuffer.push_back(src);
        
        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireBufferState(dest, ResourceStates::CopyDest);
            m_StateTracker.RequireBufferState(src, ResourceStates::CopySource);
        }
        CommitBarriers();

        m_CurrCmdList->cmdList->CopyBufferRegion(dest->resource, destOffsetBytes, src->resource, srcOffsetBytes, dataSizeBytes);
    }

    void CommandList::WriteBuffer(IBuffer *b, const void *data, size_t dataSize, uint64_t destOffsetBytes)
    {
        Buffer* buffer = Utility::CheckedCast<Buffer*>(b);
        if(buffer == nullptr || data == nullptr || dataSize == 0) 
            return;
        const auto& desc = buffer->GetDesc();

        if(desc.isConstantBuffer){
            dataSize = Math::Align(dataSize, size_t(c_ConstantBufferOffsetSizeAlignment));
        }
        auto upload = AllocateUploadBuffer(dataSize);
        memcpy(upload.mappedAddress, data, dataSize);

        m_Instance->refNativeResources.push_back(upload.resource);

        if(desc.isVolatile){
            m_VolatileBufferAddresses[std::make_pair(buffer, destOffsetBytes)] = upload.gpuAddress;
            m_HasVolatileBufferWrites = true;
        }
        else{
            m_Instance->refBuffer.push_back(buffer);

            if(m_EnableAutomaticBarriers){
                m_StateTracker.RequireBufferState(buffer, ResourceStates::CopyDest);
            }
            CommitBarriers();

            m_CurrCmdList->cmdList->CopyBufferRegion(buffer->resource, destOffsetBytes, upload.resource, upload.offset, dataSize);
        }
    }

    void CommandList::ClearBufferUInt(IBuffer *b, uint32_t clearValue)
    {
        Buffer* buffer = Utility::CheckedCast<Buffer*>(b);
        assert(buffer != nullptr);
        
        auto resources = m_Resources.lock();
        if(resources == nullptr)
            return;

        if(!buffer->GetDesc().canHaveUAVs){
            std::string msg = std::format("Cannot clear buffer {}, buffer has desc with canHaveUavs = false", 
                DebugNameToString(buffer->GetDesc().debugName));
            m_Device.GetContext().Error(msg);
            return;
        }
        
        m_Instance->refBuffer.push_back(buffer);

        uint32_t descriptorIndex = buffer->GetClearUAV();

        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireBufferState(buffer, ResourceStates::UnorderedAccess);
        }
        CommitBarriers();
        CommitDescriptorHeaps();

        uint32_t clearValues[4] = { clearValue, clearValue, clearValue, clearValue};
        m_CurrCmdList->cmdList->ClearUnorderedAccessViewUint(
            resources->shaderResourceViewHeap.GetGpuHandle(descriptorIndex),
            resources->shaderResourceViewHeap.GetCpuHandle(descriptorIndex),
            buffer->resource, clearValues, 0, nullptr);
    }

    void CommandList::SetPushConstants(const void *data, size_t byteSize)
    {
        const RootSignature* rootsig = nullptr;
        bool isGraphics = false;

        if (m_CurrGraphicsStateValid && m_CurrGraphicsState.pipeline) {
            GraphicsPipeline* pso = Utility::CheckedCast<GraphicsPipeline*>(m_CurrGraphicsState.pipeline);
            rootsig = pso->rootSignature;
            isGraphics = true;
        }
        else if (m_CurrComputeStateValid && m_CurrComputeState.pipeline) {
            ComputePipeline* pso = Utility::CheckedCast<ComputePipeline*>(m_CurrComputeState.pipeline);
            rootsig = pso->rootSignature;
            isGraphics = false;
        }
        else if (m_CurrMeshletStateValid && m_CurrMeshletState.pipeline)
        {
            MeshletPipeline* pso = Utility::CheckedCast<MeshletPipeline*>(m_CurrMeshletState.pipeline);
            rootsig = pso->rootSignature;
            isGraphics = true;
        }

        if (!rootsig || !rootsig->pushConstantByteSize)
            return;

        assert(byteSize == rootsig->pushConstantByteSize); // the validation error handles the error message
        
        if (isGraphics)
            m_CurrCmdList->cmdList->SetGraphicsRoot32BitConstants(rootsig->rootConstantsIndex, UINT(byteSize / 4), data, 0);
        else
            m_CurrCmdList->cmdList->SetComputeRoot32BitConstants(rootsig->rootConstantsIndex, UINT(byteSize / 4), data, 0);
    }

    void CommandList::SetGraphicsState(const GraphicsState &state)
    {
        GraphicsPipeline* pso = Utility::CheckedCast<GraphicsPipeline*>(state.pipeline);
        Framebuffer* framebuffer = Utility::CheckedCast<Framebuffer*>(state.framebuffer);
        assert(pso != nullptr && framebuffer != nullptr);

        const auto& psoDesc = pso->GetDesc();

        const bool currStateInvalid = !m_CurrGraphicsStateValid;

        // 判断是否更新各个状态
        const bool updatePipeline = currStateInvalid || m_CurrGraphicsState.pipeline != state.pipeline;
        const bool updateFramebuffer = currStateInvalid || m_CurrGraphicsState.framebuffer != state.framebuffer;
        const bool updateRootSig = currStateInvalid || m_CurrGraphicsState.pipeline == nullptr || 
            Utility::CheckedCast<GraphicsPipeline*>(m_CurrGraphicsState.pipeline)->rootSignature != pso->rootSignature;
        const bool updateIndirectParams = (currStateInvalid || m_CurrGraphicsState.indirectParams != state.indirectParams)
            && state.indirectParams != nullptr;
        const bool updateBlendFactor = currStateInvalid || m_CurrGraphicsState.blendConstantColor != state.blendConstantColor;
        const bool updateIndexBuffer = currStateInvalid || m_CurrGraphicsState.indexBuffer != state.indexBuffer;
        const bool updateVertexBuffer = currStateInvalid || m_CurrGraphicsState.vertexBuffers != state.vertexBuffers;
        const bool updateViewport = currStateInvalid || m_CurrGraphicsState.viewport != state.viewport;

        const uint8_t stencilRefValue = psoDesc.renderState.depthStencilState.dynamicStencilRef ?
            state.dynamicStencilRefValue : psoDesc.renderState.depthStencilState.stencilRefValue;
        const bool updateStencilRefValue = currStateInvalid || m_CurrGraphicsState.dynamicStencilRefValue != stencilRefValue;
        
        uint32_t bindingUpdateMask = 0;
        if(CommitDescriptorHeaps() || currStateInvalid || updateRootSig){
            bindingUpdateMask = uint32_t(-1);
        }
        if(bindingUpdateMask == 0){
            bindingUpdateMask = Utility::ArrayDifferenceMask(m_CurrGraphicsState.bindings, state.bindings);
        }

        auto& cmdList = m_CurrCmdList->cmdList;

        if(updateViewport){
            DX12_ViewportState viewportState = ConvertViewportState(
                psoDesc.renderState.rasterState, framebuffer->GetFramebufferInfo(), state.viewport);
            if(viewportState.numViewports > 0){
                cmdList->RSSetViewports(viewportState.numViewports, viewportState.viewports);
            }
            if(viewportState.numScissorRects > 0){
                cmdList->RSSetScissorRects(viewportState.numScissorRects, viewportState.scissorRects);
            }
        }

        if(updateFramebuffer){
            UpdateFramebuffer(framebuffer);
        }

        if(updatePipeline){
            m_Instance->refResources.push_back(pso);

            if(updateRootSig){
                cmdList->SetGraphicsRootSignature(pso->rootSignature->rootSignature);
            }
            cmdList->SetPipelineState(pso->pipelineState);
            cmdList->IASetPrimitiveTopology(ConvertPrimitiveType(psoDesc.primType, psoDesc.patchControlPoints));            
        }

        if(updateBlendFactor && pso->requiresBlendFactor){
            cmdList->OMSetBlendFactor(&state.blendConstantColor.r);
        }
        if(psoDesc.renderState.depthStencilState.stencilEnable && (updatePipeline || updateStencilRefValue)){
            cmdList->OMSetStencilRef(stencilRefValue);
        }

        if(updateIndexBuffer){
            D3D12_INDEX_BUFFER_VIEW ibv{};
            if(state.indexBuffer.buffer != nullptr){
                Buffer* buffer = Utility::CheckedCast<Buffer*>(state.indexBuffer.buffer);
                m_Instance->refBuffer.push_back(buffer);
                if(m_EnableAutomaticBarriers){
                    m_StateTracker.RequireBufferState(buffer, ResourceStates::IndexBuffer);
                }

                ibv.BufferLocation = GetBufferGpuVA(buffer, state.indexBuffer.offset);
                ibv.Format = GetDxgiFormatMapping(state.indexBuffer.format).srvFormat;
                ibv.SizeInBytes = buffer->GetDesc().byteSize - state.indexBuffer.offset;
            }

            cmdList->IASetIndexBuffer(&ibv);
        }

        if(updateVertexBuffer){
            std::array<D3D12_VERTEX_BUFFER_VIEW, c_MaxVertexAttributes> vbvs{};
            uint32_t maxVBIndex = 0;
            InputLayout* inputlayout = Utility::CheckedCast<InputLayout*>(psoDesc.inputLayout.Get());
            
            for(const auto& binding : state.vertexBuffers){
                if(binding.slot >= c_MaxVertexAttributes) continue;

                Buffer* buffer = Utility::CheckedCast<Buffer*>(binding.buffer);
                m_Instance->refBuffer.push_back(buffer);
                if(m_EnableAutomaticBarriers){
                    m_StateTracker.RequireBufferState(buffer, ResourceStates::VertexBuffer);
                }

                vbvs[binding.slot].BufferLocation = GetBufferGpuVA(buffer, binding.offset);
                vbvs[binding.slot].SizeInBytes = buffer->GetDesc().byteSize - binding.offset;
                vbvs[binding.slot].StrideInBytes = inputlayout->elementStride[binding.slot];
                maxVBIndex = std::max(binding.slot, maxVBIndex);
            }

            cmdList->IASetVertexBuffers(0, maxVBIndex + 1, vbvs.data());
        }

        // 绑定描述符
        SetResourceBindings( state.bindings, 
            bindingUpdateMask, 
            state.indirectParams, 
            updateIndirectParams, 
            pso->rootSignature, 
            true);

        CommitBarriers();

        m_CurrGraphicsStateValid = true;
        m_CurrComputeStateValid = false;
        m_CurrMeshletStateValid = false;
        m_CurrGraphicsState = state;
        m_CurrGraphicsState.dynamicStencilRefValue = stencilRefValue;
    }

    void CommandList::Draw(const DrawArguments &args)
    {
        UpdateGraphicsVolatileBuffers();
        m_CurrCmdList->cmdList->DrawInstanced(args.vertexCount, args.instanceCount, args.startVertexLocation, args.startInstanceLocation);
    }

    void CommandList::DrawIndexed(const DrawArguments &args)
    {
        UpdateGraphicsVolatileBuffers();
        m_CurrCmdList->cmdList->DrawIndexedInstanced(
            args.vertexCount, args.instanceCount, 
            args.startIndexLocation, args.startVertexLocation, args.startInstanceLocation);
    }

    void CommandList::DrawIndirect(uint32_t offsetBytes, uint32_t drawCount)
    {
        Buffer* indirectBuffer = Utility::CheckedCast<Buffer*>(m_CurrGraphicsState.indirectParams);
        assert(indirectBuffer != nullptr);

        UpdateGraphicsVolatileBuffers();
        auto commandSig = m_Device.GetContext().drawIndirectSignature;
        m_CurrCmdList->cmdList->ExecuteIndirect(
            commandSig, drawCount, indirectBuffer->resource, offsetBytes, nullptr, 0);
    }

    void CommandList::DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount)
    {        
        Buffer* indirectBuffer = Utility::CheckedCast<Buffer*>(m_CurrGraphicsState.indirectParams);
        assert(indirectBuffer != nullptr);

        UpdateGraphicsVolatileBuffers();
        auto commandSig = m_Device.GetContext().drawIndexedIndirectSignature;
        m_CurrCmdList->cmdList->ExecuteIndirect(
            commandSig, drawCount, indirectBuffer->resource, offsetBytes, nullptr, 0);
    }

    void CommandList::SetComputeState(const ComputeState &state)
    {
        ComputePipeline* pso = Utility::CheckedCast<ComputePipeline*>(state.pipeline);
        assert(pso !=nullptr);

        const bool currStateInvalid = !m_CurrComputeStateValid;
        const bool updatePipeline = currStateInvalid || m_CurrComputeState.pipeline != state.pipeline;
        const bool updateRootSig = currStateInvalid || m_CurrComputeState.pipeline == nullptr ||
            Utility::CheckedCast<ComputePipeline*>(m_CurrComputeState.pipeline)->rootSignature != pso->rootSignature;
        const bool updateIndirectParams = (currStateInvalid || m_CurrComputeState.indirectParams != state.indirectParams)
            && state.indirectParams != nullptr;
        
        uint32_t bindingUpdateMask = 0;
        if(CommitDescriptorHeaps() || currStateInvalid || updateRootSig){
            bindingUpdateMask = uint32_t(-1);
        }
        if(bindingUpdateMask == 0){
            bindingUpdateMask = Utility::ArrayDifferenceMask(m_CurrComputeState.bindings, state.bindings);
        }

        if(updatePipeline){
            m_Instance->refResources.push_back(pso);

            if(updateRootSig){
                m_CurrCmdList->cmdList->SetComputeRootSignature(pso->rootSignature->rootSignature);
            }
            m_CurrCmdList->cmdList->SetPipelineState(pso->pipelineState);
        }

        SetResourceBindings(
            state.bindings, 
            bindingUpdateMask, 
            state.indirectParams, 
            updateIndirectParams, 
            pso->rootSignature,
            false);

        CommitBarriers();

        m_CurrComputeState = state;
        m_CurrComputeStateValid = true;
        m_CurrGraphicsStateValid = false;
        m_CurrMeshletStateValid = false;
        m_CurrRayTracingStateValid = false;
    }

    void CommandList::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        UpdateComputeVolatileBuffers();
        m_CurrCmdList->cmdList->Dispatch(groupsX, groupsY, groupsZ);
    }

    void CommandList::DispatchIndirect(uint32_t offsetBytes)
    {
        Buffer* indirectBuffer = Utility::CheckedCast<Buffer*>(m_CurrComputeState.indirectParams);
        UpdateComputeVolatileBuffers();
        auto signature = m_Device.GetContext().dispatchIndirectSignature;
        m_CurrCmdList->cmdList->ExecuteIndirect(signature, 1, indirectBuffer->resource, offsetBytes, nullptr, 0);
    }

    void CommandList::SetMeshletState(const MeshletState &state)
    {
        MeshletPipeline* pso = Utility::CheckedCast<MeshletPipeline*>(state.pipeline);
        Framebuffer* framebuffer = Utility::CheckedCast<Framebuffer*>(state.framebuffer);
        assert(pso != nullptr && framebuffer != nullptr);

        const auto& psoDesc = pso->GetDesc();

        const bool currStateInvalid = !m_CurrMeshletStateValid;

        // 判断是否更新各个状态
        const bool updatePipeline = currStateInvalid || m_CurrMeshletState.pipeline != state.pipeline;
        const bool updateFramebuffer = currStateInvalid || m_CurrMeshletState.framebuffer != state.framebuffer;
        const bool updateRootSig = currStateInvalid || m_CurrMeshletState.pipeline == nullptr || 
            Utility::CheckedCast<MeshletPipeline*>(m_CurrMeshletState.pipeline)->rootSignature != pso->rootSignature;
        const bool updateIndirectParams = (currStateInvalid || m_CurrMeshletState.indirectParams != state.indirectParams)
            && state.indirectParams != nullptr;
        const bool updateBlendFactor = currStateInvalid || m_CurrMeshletState.blendConstantColor != state.blendConstantColor;
        const bool updateViewport = currStateInvalid || m_CurrMeshletState.viewport != state.viewport;

        const uint8_t stencilRefValue = psoDesc.renderState.depthStencilState.dynamicStencilRef ?
            state.dynamicStencilRefValue : psoDesc.renderState.depthStencilState.stencilRefValue;
        const bool updateStencilRefValue = currStateInvalid || m_CurrMeshletState.dynamicStencilRefValue != stencilRefValue;
        
        uint32_t bindingUpdateMask = 0;
        if(CommitDescriptorHeaps() || currStateInvalid || updateRootSig){
            bindingUpdateMask = uint32_t(-1);
        }
        if(bindingUpdateMask == 0){
            bindingUpdateMask = Utility::ArrayDifferenceMask(m_CurrMeshletState.bindings, state.bindings);
        }

        auto& cmdList = m_CurrCmdList->cmdList;

        if(updateViewport){
            DX12_ViewportState viewportState = ConvertViewportState(
                psoDesc.renderState.rasterState, framebuffer->GetFramebufferInfo(), state.viewport);
            if(viewportState.numViewports > 0){
                cmdList->RSSetViewports(viewportState.numViewports, viewportState.viewports);
            }
            if(viewportState.numScissorRects > 0){
                cmdList->RSSetScissorRects(viewportState.numScissorRects, viewportState.scissorRects);
            }
        }

        if(updateFramebuffer){
            UpdateFramebuffer(framebuffer);
        }

        if(updatePipeline){
            m_Instance->refResources.push_back(pso);

            if(updateRootSig){
                cmdList->SetGraphicsRootSignature(pso->rootSignature->rootSignature);
            }
            cmdList->SetPipelineState(pso->pipelineState);
            cmdList->IASetPrimitiveTopology(ConvertPrimitiveType(psoDesc.primType, 0));            
        }

        if(updateBlendFactor && pso->requiresBlendFactor){
            cmdList->OMSetBlendFactor(&state.blendConstantColor.r);
        }
        if(psoDesc.renderState.depthStencilState.stencilEnable && (updatePipeline || updateStencilRefValue)){
            cmdList->OMSetStencilRef(stencilRefValue);
        }

        // 绑定描述符
        SetResourceBindings( state.bindings, 
            bindingUpdateMask, 
            state.indirectParams, 
            updateIndirectParams, 
            pso->rootSignature, 
            true);

        CommitBarriers();

        m_CurrGraphicsStateValid = false;
        m_CurrComputeStateValid = false;
        m_CurrMeshletStateValid = true;
        m_CurrRayTracingStateValid = false;
        m_CurrMeshletState = state;
        m_CurrMeshletState.dynamicStencilRefValue = stencilRefValue;
    }

    void CommandList::DispatchMesh(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        UpdateGraphicsVolatileBuffers();
        m_CurrCmdList->cmdList6->DispatchMesh(groupsX, groupsY, groupsZ);
    }

    //////////////////////////////////////////////////////////////////////////
    // 光线追踪（DXR）
    //////////////////////////////////////////////////////////////////////////
    void CommandList::SetRayTracingState(const RT::State& state)
    {
        if (state.shaderTable == nullptr || m_CurrCmdList == nullptr || m_CurrCmdList->cmdList4 == nullptr)
            return;

        ShaderTable* shaderTable = Utility::CheckedCast<ShaderTable*>(state.shaderTable);
        RayTracingPipeline* pso = Utility::CheckedCast<RayTracingPipeline*>(shaderTable->GetPipeline());
        auto resources = m_Resources.lock();
        if (shaderTable == nullptr || pso == nullptr || resources == nullptr)
            return;

        const bool shaderTableCached = shaderTable->GetDesc().isCached;
        ShaderTableState& shaderTableState = GetShaderTableState(shaderTable);
        // 判断当前 shader table 是否需要重新构建
        const bool rebuildShaderTable = !shaderTable->IsStateValid(shaderTableState, *resources);
        if (rebuildShaderTable) {
            const size_t shaderTableSize = shaderTable->GetShaderTableSize();

            if (shaderTableCached && (shaderTable->cache == nullptr || shaderTable->cache->GetDesc().byteSize < shaderTableSize)) {
                m_Device.GetContext().Error("Required shader table size is larger than the allocated cache. Increase ShaderTableDesc::maxEntries.");
                return;
            }

            const auto upload = AllocateUploadBuffer(shaderTableSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
            if (upload.resource == nullptr || upload.mappedAddress == nullptr)
            {
                m_Device.GetContext().Error("Couldn't allocate an upload buffer for the shader table");
                return;
            }

            const D3D12_GPU_VIRTUAL_ADDRESS shaderTableGpuAddress = shaderTableCached
                ? shaderTable->cache->GetGpuVirtualAddress()
                : upload.gpuAddress;
            shaderTable->Bake(static_cast<uint8_t*>(upload.mappedAddress), shaderTableGpuAddress, *resources, shaderTableState);

            if (shaderTableCached)
            {
                SetBufferState(shaderTable->cache.Get(), ResourceStates::CopyDest);
                CommitBarriers();

                auto* cacheBuffer = Utility::CheckedCast<Buffer*>(shaderTable->cache.Get());
                m_CurrCmdList->cmdList->CopyBufferRegion(cacheBuffer->resource.Get(), 0, upload.resource, upload.offset, shaderTableSize);
            }
        }

        if (shaderTableCached)
        {
            SetBufferState(shaderTable->cache.Get(), ResourceStates::ShaderResource);
        }

        if (shaderTableCached || rebuildShaderTable)
        {
            m_Instance->refResources.push_back(shaderTable);
        }

        // 设置光线追踪状态对象（StateObject1）
        m_CurrCmdList->cmdList4->SetPipelineState1(pso->GetStateObject());

        // 设置全局根签名
        RootSignature* globalRS = pso->GetGlobalRootSignature();
        if (globalRS != nullptr) {
            m_CurrCmdList->cmdList->SetComputeRootSignature(globalRS->rootSignature);
        }

        // 绑定全局根签名对应的绑定集合
        if (!state.bindingSets.empty()) {
            BindingSetVector bindingSets;
            for (IBindingSet* bs : state.bindingSets) {
                if (bs != nullptr) bindingSets.push_back(RefPtr<IBindingSet>(bs));
            }
            if (!bindingSets.empty()) {
                CommitDescriptorHeaps();
                uint32_t bindingUpdateMask = uint32_t(-1);
                SetResourceBindings(bindingSets, bindingUpdateMask, nullptr, false, globalRS, false);
            }
        }

        CommitBarriers();

        m_CurrRayTracingState = state;
        m_CurrRayTracingStateValid = true;
        m_CurrGraphicsStateValid = false;
        m_CurrComputeStateValid = false;
        m_CurrMeshletStateValid = false;
    }

    void CommandList::DispatchRays(const RT::DispatchRaysArguments& args)
    {
        UpdateComputeVolatileBuffers();

        if (!m_CurrRayTracingStateValid)
        {
            m_Device.GetContext().Error("setRayTracingState must be called before dispatchRays");
            return;
        }

        ShaderTableState& shaderTableState = GetShaderTableState(m_CurrRayTracingState.shaderTable);

        auto desc = shaderTableState.dispatchRaysTemplate;
        desc.Width = args.width;
        desc.Height = args.height;
        desc.Depth = args.depth;

        m_CurrCmdList->cmdList4->DispatchRays(&desc);
    }

    void CommandList::BuildBottomLevelAccelStruct(RT::IAccelStruct *as, RT::AccelStructBuildFlags buildFlags)
    {
        AccelStruct* accel = Utility::CheckedCast<AccelStruct*>(as);
        if(accel == nullptr || accel->dataBuffer == nullptr || m_CurrCmdList->cmdList4 == nullptr)
            return;

        const auto& desc = accel->GetDesc();
        if(desc.isTopLevel){
            m_Device.GetContext().Error("BuildBottomLevelAccelStruct called on a top-level acceleration structure");
            return;
        }

        const auto& dataBuffer = accel->dataBuffer;

        // 转换资源的状态
        for(const auto& geom : desc.bottomLevelGeometries){
            if(geom.geometryType == RT::GeometryType::Triangles){
                const auto& triangle = geom.geometryData.triangles;
                if(m_EnableAutomaticBarriers){
                    if (triangle.vertexBuffer != nullptr) {
                        m_StateTracker.RequireBufferState(triangle.vertexBuffer, ResourceStates::AccelStructBuildInput);
                    }
                    if (triangle.indexBuffer != nullptr) {
                        m_StateTracker.RequireBufferState(triangle.indexBuffer, ResourceStates::AccelStructBuildInput);
                    }
                }

                if(m_Instance != nullptr){
                    if(triangle.vertexBuffer != nullptr)
                        m_Instance->refBuffer.push_back(Utility::CheckedCast<Buffer*>(triangle.vertexBuffer));
                    if(triangle.indexBuffer != nullptr)
                        m_Instance->refBuffer.push_back(Utility::CheckedCast<Buffer*>(triangle.indexBuffer));
                }
            }
            else if(geom.geometryType == RT::GeometryType::AABBs){
                const auto& aabb = geom.geometryData.aabbs;
                if(m_EnableAutomaticBarriers){
                    m_StateTracker.RequireBufferState(aabb.buffer, ResourceStates::AccelStructBuildInput);
                }

                if(m_Instance != nullptr && aabb.buffer != nullptr){
                    m_Instance->refBuffer.push_back(Utility::CheckedCast<Buffer*>(aabb.buffer));
                }
            }
        }
        CommitBarriers();

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geoDescs{};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs = GetAccelerationStructureBuildInputs(desc, geoDescs);
        buildInputs.Flags = ConvertAccelerationStructureBuildFlags(buildFlags);
        assert(buildInputs.NumDescs == geoDescs.size());

        // 分配 Transform 的缓冲区
        for(size_t i = 0; i < desc.bottomLevelGeometries.size(); ++i){
            const auto& geom = desc.bottomLevelGeometries[i];
            auto& geoDesc = geoDescs[i];
            if(auto& transform = geoDesc.Triangles.Transform3x4; transform != 0){
                auto transformBuffer = AllocateUploadBuffer(sizeof(RT::AffineTransform), D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT);
                memcpy(transformBuffer.mappedAddress, &geom.transform, sizeof(RT::AffineTransform));
                transform = transformBuffer.gpuAddress;
            }
        }

        auto& context = m_Device.GetContext();
        if(context.device5 == nullptr)
            return;
        
        // 获取加速结构的构建信息
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
        context.device5->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

        if(prebuildInfo.ResultDataMaxSizeInBytes > dataBuffer->GetDesc().byteSize){
            std::string msg = std::format("The buffer size of the bottom-level acceleration structure is too small. "
                "Required size: {}, actual size: {}", prebuildInfo.ResultDataMaxSizeInBytes, dataBuffer->GetDesc().byteSize);
            context.Error(msg);
            return;
        }

        // 创建需要的 scratch buffer
        bool performUpdate = HasFlags(buildFlags, RT::AccelStructBuildFlags::AllowUpdate);
        uint64_t scratchSize = performUpdate ? prebuildInfo.UpdateScratchDataSizeInBytes : prebuildInfo.ScratchDataSizeInBytes;
        auto scratchBuffer = AllocateGpuBuffer(scratchSize, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireBufferState(dataBuffer, ResourceStates::AccelStructWrite);
        }
        CommitBarriers();

        // 创建加速结构
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
        buildDesc.DestAccelerationStructureData = dataBuffer->GetGpuVirtualAddress();
        buildDesc.Inputs = buildInputs;
        buildDesc.SourceAccelerationStructureData = performUpdate ? dataBuffer->GetGpuVirtualAddress() : 0;
        buildDesc.ScratchAccelerationStructureData = scratchBuffer.gpuAddress;
        m_CurrCmdList->cmdList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

        if(accel->GetDesc().trackLiveness){
            m_Instance->refResources.push_back(accel);
        }
    }

    void CommandList::BuildTopLevelAccelStruct(RT::IAccelStruct *as, std::span<const RT::InstanceDesc> instances, RT::AccelStructBuildFlags buildFlags)
    {
        AccelStruct* accel = Utility::CheckedCast<AccelStruct*>(as);
        if(accel == nullptr || !accel->GetDesc().isTopLevel || accel->dataBuffer == nullptr || m_CurrCmdList->cmdList4 == nullptr)
            return;
        
        accel->bottomLevelASes.clear();
        accel->instanceDescs.resize(instances.size());
        for(const auto& [index, instance] : instances | std::views::enumerate){
            AccelStruct* blas = Utility::CheckedCast<AccelStruct*>(instance.bottomLevelAS);
            auto& instanceDesc = accel->instanceDescs[index];
            if(blas != nullptr){
                memcpy(&instanceDesc, &instance, sizeof(instance));
                instanceDesc.AccelerationStructure = blas->dataBuffer->GetGpuVirtualAddress();

                if(m_EnableAutomaticBarriers){
                    m_StateTracker.RequireBufferState(blas->dataBuffer, ResourceStates::AccelStructBuildBlas);
                }
                if(blas->GetDesc().trackLiveness){
                    accel->bottomLevelASes.push_back(blas);
                }
            }
            else{
                instanceDesc.AccelerationStructure = 0;
            }
        }

        // 将实例描述符上传到 GPU
        size_t instanceDescsSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * accel->instanceDescs.size();
        auto uploadBuffer = AllocateUploadBuffer(instanceDescsSize, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);
        memcpy(uploadBuffer.mappedAddress, accel->instanceDescs.data(), instanceDescsSize);

        BuildTopLevelAccelStructInternal(accel, uploadBuffer.gpuAddress, instances.size(), buildFlags);
    }

    void CommandList::BuildTopLevelAccelStructFromBuffer(RT::IAccelStruct *as, IBuffer *instanceBuffer, uint64_t instanceBufferOffset, size_t numInstances, RT::AccelStructBuildFlags buildFlags)
    {
        AccelStruct* accel = Utility::CheckedCast<AccelStruct*>(as);

        accel->bottomLevelASes.clear();
        accel->instanceDescs.clear();

        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireBufferState(instanceBuffer, ResourceStates::AccelStructBuildInput);
            m_StateTracker.RequireBufferState(accel->dataBuffer, ResourceStates::AccelStructWrite);
        }
        CommitBarriers();

        if(m_Instance != nullptr && accel->GetDesc().trackLiveness){
            m_Instance->refBuffer.push_back(Utility::CheckedCast<Buffer*>(instanceBuffer));
        }

        BuildTopLevelAccelStructInternal(accel, GetBufferGpuVA(instanceBuffer, instanceBufferOffset), numInstances, buildFlags);
    }

    void CommandList::BeginTimerQuery(ITimerQuery *_query)
    {
        TimerQuery* query = Utility::CheckedCast<TimerQuery*>(_query);
        m_Instance->refTimerQuery.push_back(query);
        m_CurrCmdList->cmdList->EndQuery(m_Device.GetContext().timerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, query->beginQueryIndex);        
    }

    void CommandList::EndTimerQuery(ITimerQuery *_query)
    {
        TimerQuery* query = Utility::CheckedCast<TimerQuery*>(_query);
        const auto& context = m_Device.GetContext();

        m_Instance->refTimerQuery.push_back(query);

        m_CurrCmdList->cmdList->EndQuery(context.timerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, query->endQueryIndex);
        m_CurrCmdList->cmdList->ResolveQueryData(context.timerQueryHeap,
            D3D12_QUERY_TYPE_TIMESTAMP,
            query->beginQueryIndex,
            2,
            context.timerQueryResolveBuffer->resource,
            query->beginQueryIndex * 8);
    }

    void CommandList::BeginEvent(const char *name)
    {
        PIXBeginEvent(m_CurrCmdList->cmdList, 0, name);
    }

    void CommandList::EndEvent()
    {
        PIXEndEvent(m_CurrCmdList->cmdList);
    }

    DynamicResourceLocation CommandList::AllocateUploadBuffer(size_t size, size_t alignment)
    {
        return m_CurrCmdList->uploadBufferAllocator->Allocate(size, alignment);
    }

    DynamicResourceLocation CommandList::AllocateGpuBuffer(size_t size, size_t alignment)
    {
        return m_CurrCmdList->gpuBufferAllocator->Allocate(size, alignment);
    }

    bool CommandList::CommitDescriptorHeaps()
    {
        auto resources = m_Resources.lock();
        if(resources == nullptr) 
            return false;
        
        // 由于描述符堆扩展后原来的描述符句柄会失效，因此需要重新绑定
        auto heapSRV = resources->shaderResourceViewHeap.GetShaderVisibleHeap();
        auto heapSampler = resources->samplerHeap.GetShaderVisibleHeap();

        if(m_CurrSRVHeap == heapSRV && m_CurrSamplerHeap == heapSampler) return false;

        m_Instance->refNativeResources.push_back(heapSRV);
        m_Instance->refNativeResources.push_back(heapSampler);

        ID3D12DescriptorHeap* heaps[2] = { heapSRV, heapSampler };
        m_CurrCmdList->cmdList->SetDescriptorHeaps(2, heaps);

        m_CurrSRVHeap = heapSRV;
        m_CurrSamplerHeap = heapSampler;

        return true;
    }

    D3D12_GPU_VIRTUAL_ADDRESS CommandList::GetBufferGpuVA(IBuffer *b, uint64_t offset)
    {
        Buffer* buffer = Utility::CheckedCast<Buffer*>(b);

        D3D12_GPU_VIRTUAL_ADDRESS address = buffer->GetDesc().isVolatile ? 
            m_VolatileBufferAddresses[std::make_pair(buffer, offset)] : (buffer->GetGpuVirtualAddress() + offset);
        return address;
    }

    void CommandList::UpdateGraphicsVolatileBuffers()
    {
        if(!m_HasVolatileBufferWrites) return;

        for(auto& binding : m_GraphicsVolatileBuffers){
            D3D12_GPU_VIRTUAL_ADDRESS address = m_VolatileBufferAddresses[std::make_pair(binding.buffer, binding.offset)];
            if(address != binding.address){
                m_CurrCmdList->cmdList->SetGraphicsRootConstantBufferView(binding.rootParaIndex, address);
                binding.address = address;
            }
        }
        
        m_HasVolatileBufferWrites = false;
    }

    void CommandList::UpdateComputeVolatileBuffers()
    {
        if(!m_HasVolatileBufferWrites) return;

        for(auto& binding : m_ComputeVolatileBuffers){
            D3D12_GPU_VIRTUAL_ADDRESS address = m_VolatileBufferAddresses[std::make_pair(binding.buffer, binding.offset)];
            if(address != binding.address){
                m_CurrCmdList->cmdList->SetComputeRootConstantBufferView(binding.rootParaIndex, address);
                binding.address = address;
            }
        }
        
        m_HasVolatileBufferWrites = false;
    }

    void CommandList::SetEnableAutomaticBarriers(bool enable)
    {
        m_EnableAutomaticBarriers = enable;
    }

    void CommandList::SetEnableUavBarriersForTexture(ITexture *texture, bool enableBarriers)
    {
        m_StateTracker.SetEnableUavBarrierForTexture(texture, enableBarriers);
    }

    void CommandList::SetEnableUavBarriersForBuffer(IBuffer *b, bool enableBarriers)
    {
        m_StateTracker.SetEnableUavBarrierForBuffer(b, enableBarriers);
    }

    void CommandList::SetTextureState(ITexture *texture, TextureSubresourceSet subresources, ResourceStates stateBits)
    {
        // 由于 Barrier 可能会使用 Texture，因此需要记录
        m_Instance->refResources.push_back(texture);
        m_StateTracker.RequireTextureState(texture, subresources, stateBits);
    }

    void CommandList::SetBufferState(IBuffer *b, ResourceStates stateBits)
    {
        m_Instance->refBuffer.push_back(Utility::CheckedCast<Buffer*>(b));
        m_StateTracker.RequireBufferState(b, stateBits);
    }

    void CommandList::SetAccelStructState(RT::IAccelStruct *as, ResourceStates stateBits)
    {
        AccelStruct* accel = Utility::CheckedCast<AccelStruct*>(as);
        if(accel != nullptr && accel->dataBuffer != nullptr){
            m_StateTracker.RequireBufferState(accel->dataBuffer, stateBits);
            if(m_Instance != nullptr){
                m_Instance->refResources.push_back(accel);
            }
        }
    }

    void CommandList::SetResourceStatesForBindingSet(IBindingSet *bindingSet)
    {
        if(bindingSet->GetDesc() == nullptr) return;    // Bindless
        
        auto setTexState = [this] (const BindingSetItem& binding, ResourceStates state){
            auto tex = Utility::CheckedCast<Texture*>(binding.resourceHandle);
            m_StateTracker.RequireTextureState(tex, binding.subresources, state);
        };
        auto setBufferState = [this] (const auto& binding, ResourceStates state){
            auto buffer = Utility::CheckedCast<Buffer*>(binding.resourceHandle);
            m_StateTracker.RequireBufferState(buffer, state);
        };

        for(const auto& binding : bindingSet->GetDesc()->bindings){
            switch (binding.type) {
            case ResourceType::Texture_SRV:
                setTexState(binding, ResourceStates::ShaderResource); break;
            case ResourceType::Texture_UAV:
                setTexState(binding, ResourceStates::UnorderedAccess); break;
            case ResourceType::RawBuffer_SRV:
            case ResourceType::TypedBuffer_SRV:
            case ResourceType::StructuredBuffer_SRV:
                setBufferState(binding, ResourceStates::ShaderResource); break;
            case ResourceType::RawBuffer_UAV:
            case ResourceType::TypedBuffer_UAV:
            case ResourceType::StructuredBuffer_UAV:
                setBufferState(binding, ResourceStates::UnorderedAccess); break;
            case ResourceType::ConstantBuffer:
                if(m_Desc.queueType == CommandQueueType::Graphics) {
                    setBufferState(binding, ResourceStates::ConstantBuffer);
                }
                break;
            case ResourceType::RayTracingAccelStruct:{
                // 加速结构作为 SRV 使用时需处于可读的加速结构状态
                auto buffer = Utility::CheckedCast<Buffer*>(binding.resourceHandle);
                m_StateTracker.RequireBufferState(buffer, ResourceStates::AccelStructRead);
                break;
            }
            default:
                break;
            }
        }
    }

    ResourceStates CommandList::GetTextureSubresourceState(ITexture *texture, uint32_t arraySlice, uint32_t mipLevel)
    {
        return m_StateTracker.GetTextureSubresourceState(texture, mipLevel, arraySlice);
    }

    ResourceStates CommandList::GetBufferState(IBuffer *b)
    {
        return m_StateTracker.GetBufferState(b);
    }

    void CommandList::CommitBarriers()
    {
        // Compute 队列中不允许的状态掩码，需要从 barrier 中剥离
        constexpr D3D12_RESOURCE_STATES invalidComputeQueueResourceStates =
            ( D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            | D3D12_RESOURCE_STATE_RENDER_TARGET
            | D3D12_RESOURCE_STATE_DEPTH_READ
            | D3D12_RESOURCE_STATE_DEPTH_WRITE
            | D3D12_RESOURCE_STATE_STREAM_OUT
            | D3D12_RESOURCE_STATE_RESOLVE_DEST
            | D3D12_RESOURCE_STATE_RESOLVE_SOURCE
            | D3D12_RESOURCE_STATE_PRESENT
            | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
            | D3D12_RESOURCE_STATE_INDEX_BUFFER
            | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
            | D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE );

        std::vector<TextureBarrier> texBarrier{};
        std::vector<BufferBarrier> bufferBarrier{};
        m_StateTracker.ConsumeBarriers(texBarrier, bufferBarrier);
        size_t barrierCount = texBarrier.size() + bufferBarrier.size();
        if(barrierCount == 0) return;

        std::vector<D3D12_RESOURCE_BARRIER> barriers{};
        barriers.reserve(barrierCount);

        // 从 D3D12 资源状态中移除 Compute 队列不允许的状态
        auto filterComputeQueueStates = [&](D3D12_RESOURCE_STATES states) -> D3D12_RESOURCE_STATES {
            if (m_Desc.queueType == CommandQueueType::Compute) {
                states &= ~invalidComputeQueueResourceStates;
            }
            return states;
        };

        for(const auto& barrier : texBarrier){
            D3D12_RESOURCE_BARRIER d3dBarrier{};
            const D3D12_RESOURCE_STATES rawBeforeState = ConvertResourceStates(barrier.stateBefore);
            const D3D12_RESOURCE_STATES rawAfterState = ConvertResourceStates(barrier.stateAfter);
            const D3D12_RESOURCE_STATES beforeState = filterComputeQueueStates(rawBeforeState);
            const D3D12_RESOURCE_STATES afterState = filterComputeQueueStates(rawAfterState);

            auto texture = Utility::CheckedCast<Texture*>(barrier.texture);
            const auto& desc = texture->GetDesc();

            if(beforeState != afterState){
                d3dBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                d3dBarrier.Transition.pResource = texture->resource.Get();
                d3dBarrier.Transition.StateBefore = beforeState;
                d3dBarrier.Transition.StateAfter = afterState;
                if(barrier.entireTexture){
                    d3dBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    barriers.push_back(std::move(d3dBarrier));
                }
                else{
                    for(uint8_t plane = 0; plane < texture->planeCount; ++plane){
                        d3dBarrier.Transition.Subresource = CalculateSubresource(
                            barrier.mipLevel, barrier.arraySlice, plane, desc.mipLevels, desc.arraySize);
                        barriers.push_back(std::move(d3dBarrier));
                    }
                }
            }
            else if(HasFlags(afterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)) { // UAV Barrier
                d3dBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                d3dBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                d3dBarrier.UAV.pResource = texture->resource.Get();
                barriers.push_back(std::move(d3dBarrier));
            }
        }
        for(const auto& barrier : bufferBarrier){
            D3D12_RESOURCE_BARRIER d3dbarrier{};
            const D3D12_RESOURCE_STATES rawBeforeState = ConvertResourceStates(barrier.stateBefore);
            const D3D12_RESOURCE_STATES rawAfterState = ConvertResourceStates(barrier.stateAfter);
            const D3D12_RESOURCE_STATES beforeState = filterComputeQueueStates(rawBeforeState);
            const D3D12_RESOURCE_STATES afterState = filterComputeQueueStates(rawAfterState);

            Buffer* buffer = Utility::CheckedCast<Buffer*>(barrier.buffer);
            if(auto state = beforeState | afterState; beforeState != afterState && 
                !HasFlags(state, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)){
                d3dbarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                d3dbarrier.Transition.pResource = buffer->resource.Get();
                d3dbarrier.Transition.StateBefore = beforeState;
                d3dbarrier.Transition.StateAfter = afterState;
                d3dbarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barriers.push_back(std::move(d3dbarrier));
            }
            else if((barrier.stateBefore == ResourceStates::AccelStructWrite
                    && HasFlags(barrier.stateAfter, ResourceStates::AccelStructRead | ResourceStates::AccelStructBuildBlas))
                || (barrier.stateAfter == ResourceStates::AccelStructWrite
                    && HasFlags(barrier.stateBefore, ResourceStates::AccelStructRead | ResourceStates::AccelStructBuildBlas))
                || HasFlags(afterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)){
                d3dbarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                d3dbarrier.UAV.pResource = buffer->resource.Get();
                barriers.push_back(std::move(d3dbarrier));
            }
        }
        
        if(barriers.size() > 0){
            m_CurrCmdList->cmdList->ResourceBarrier(barriers.size(), barriers.data());
        }
    }

    IDevice *CommandList::GetDevice()
    {
        return &m_Device;
    }
 
    void CommandList::SetResourceBindings(
        const BindingSetVector &bindings, 
        uint32_t bindingUpdateMask, 
        IBuffer *indirectParams, 
        bool updateIndirectParams, 
        const RootSignature *rootSignature,
        bool isGraphics)
    {
        auto resources = m_Resources.lock();
        if(resources == nullptr) 
            return;

        if(updateIndirectParams && m_EnableAutomaticBarriers){
            m_StateTracker.RequireBufferState(indirectParams, ResourceStates::IndirectArgument);
            m_Instance->refBuffer.push_back(Utility::CheckedCast<Buffer*>(indirectParams));
        }

        uint32_t bindingMask = (1 << bindings.size()) - 1;
        if((bindingMask & bindingUpdateMask) == bindingMask){
            m_HasVolatileBufferWrites = false;
        }

        if(bindingUpdateMask == 0) return;
        const auto& cmdList = m_CurrCmdList->cmdList;
        auto setConstantBuffer = [&cmdList, isGraphics](UINT index, D3D12_GPU_VIRTUAL_ADDRESS address){
            if(isGraphics){
                cmdList->SetGraphicsRootConstantBufferView(index, address);
            }
            else{
                cmdList->SetComputeRootConstantBufferView(index, address);
            }
        };
        auto setDescriptorTable = [&cmdList, isGraphics](UINT index, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle){
            if(isGraphics){
                cmdList->SetGraphicsRootDescriptorTable(index, gpuHandle);
            }
            else{
                cmdList->SetComputeRootDescriptorTable(index, gpuHandle);
            }
        };

        StaticVector<VolatileBufferBinding, c_MaxVolatileConstantBuffers> newVolatileBuffers{};
        for(size_t bindingIndex = 0; bindingIndex < bindings.size(); ++bindingIndex){
            const auto& binding = bindings[bindingIndex];
            if(binding == nullptr) continue;

            const bool updateBinding = ((1 << bindingIndex) & bindingUpdateMask) != 0;
            // 获取根签名对应的绑定布局
            const auto& [rootIndexOffset, bindingLayout] = rootSignature->pipelineLayouts[bindingIndex];

            if(binding->GetDesc() != nullptr){
                BindingSet* bindingSet = Utility::CheckedCast<BindingSet*>(binding.Get());
                assert(bindingLayout == bindingSet->GetLayout());

                // 绑定常量缓冲区
                for(const auto& [cbIndex, volatileCB, bufferOffset] : bindingSet->rootParametersVolatileCBs){
                    uint32_t rootIndex = cbIndex + rootIndexOffset;
                    if(volatileCB == nullptr) continue;
                    GpuVirtualAddress address = GetBufferGpuVA(volatileCB, bufferOffset);

                    const auto& cbDesc = volatileCB->GetDesc();
                    if(cbDesc.isVolatile){
                        if(address == 0){
                            std::string msg = std::format("Attempted use of a volatile constant buffer {} before it was written into",
                                DebugNameToString(volatileCB->GetDesc().debugName));
                            m_Device.GetContext().Error(msg);
                            continue;       
                        }
                        if (updateBinding || address != m_GraphicsVolatileBuffers[newVolatileBuffers.size()].address) {
                            setConstantBuffer(rootIndex, address);
                        }

                        VolatileBufferBinding cbBinding{};
                        cbBinding.rootParaIndex = rootIndex;
                        cbBinding.buffer = volatileCB;
                        cbBinding.offset = bufferOffset;
                        cbBinding.address = address;
                        newVolatileBuffers.push_back(std::move(cbBinding));
                    }
                    else if(updateBinding){
                        setConstantBuffer(rootIndex, address);
                    }
                }

                if (updateBinding) {
                    if (bindingSet->hasSamplers) {
                        setDescriptorTable( rootIndexOffset + bindingSet->bindingLayout->rootParameterIndexSamplers,
                            resources->samplerHeap.GetGpuHandle(bindingSet->descriptorIndexSamplers));
                    }
                    if (bindingSet->hasSRVs) {
                        setDescriptorTable(rootIndexOffset + bindingSet->bindingLayout->rootParameterIndexSRVs,
                            resources->shaderResourceViewHeap.GetGpuHandle(bindingSet->descriptorIndexSRVs));
                    }
                    if(bindingSet->GetDesc()->trackLiveness){
                        m_Instance->refResources.push_back(bindingSet);
                    }
                }

                if (m_EnableAutomaticBarriers && 
                    (updateBinding || bindingSet->hasUAVs) && 
                    m_Desc.queueType == CommandQueueType::Graphics) {
                    SetResourceStatesForBindingSet(bindingSet);
                }
            }
            else if(rootIndexOffset != c_InvalidRootParameterIndex){  // DecriptorTable
                DescriptorTable* table = Utility::CheckedCast<DescriptorTable*>(binding.Get());
                auto gpuHandle = resources->shaderResourceViewHeap.GetGpuHandle(table->firstDescriptor);
                setDescriptorTable(rootIndexOffset, gpuHandle);
            }
        }

        auto& volatileBuffers = isGraphics ? m_GraphicsVolatileBuffers : m_ComputeVolatileBuffers;
        volatileBuffers = std::move(newVolatileBuffers);
    }


    std::shared_ptr<CommandListInstance> CommandList::Executed(CommandQueue &queue)
    {
        std::shared_ptr<CommandListInstance> instance = m_Instance;
        instance->fence = queue.GetFence();
        instance->submitFenceValue = queue.GetNextFenceValue() - 1;
        m_Instance = nullptr;

        m_CurrCmdList->lastSubmittedFenceValue = instance->submitFenceValue;
        // 释放命令列表
        assert(InternalCommandList::ReleaseCommandList(m_CurrCmdList));
        m_CurrCmdList = nullptr;

        for (const auto& buffer : instance->refBuffer) {
            buffer->lastUseFence = queue.GetFence();
            buffer->lastUseFenceValue = instance->submitFenceValue;
        }
        for (const auto& timer : instance->refTimerQuery) {
            timer->fence = queue.GetFence();
            timer->fenceCounter = instance->submitFenceValue;
            timer->resolved = false;
            timer->started = true;
        }
        
        return instance;
    }
    



    void CommandList::ClearStateCache()
    {
        m_CurrGraphicsStateValid = false;
        m_CurrComputeStateValid = false;
        m_CurrMeshletStateValid = false;
        m_CurrRayTracingStateValid = false;
        m_HasVolatileBufferWrites = false;
        m_CurrSRVHeap = nullptr;
        m_CurrSamplerHeap = nullptr;
        m_GraphicsVolatileBuffers.resize(0);
        m_ComputeVolatileBuffers.resize(0);
        m_ShaderTableStates.clear();
    }

    void CommandList::UpdateFramebuffer(Framebuffer *framebuffer)
    {
        auto resources = m_Resources.lock();
        if(resources == nullptr) 
            return;
        
        if(m_EnableAutomaticBarriers){
            SetResourceStatesForFramebuffer(framebuffer);
        }
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> RTVs(framebuffer->RTVs.size());
        for(int i = 0; i < framebuffer->RTVs.size(); ++i){
            RTVs[i] = resources->renderTargetViewHeap.GetCpuHandle(framebuffer->RTVs[i]);
        }

        bool hasDepth = framebuffer->GetDesc().depthAttachment.Valid();
        D3D12_CPU_DESCRIPTOR_HANDLE DSV;
        if (hasDepth) {
            DSV = resources->depthStencilViewHeap.GetCpuHandle(framebuffer->DSV);
        }
        m_CurrCmdList->cmdList->OMSetRenderTargets(
            RTVs.size(), RTVs.size() == 0 ? nullptr : RTVs.data(), false, hasDepth ? &DSV : nullptr);

        m_Instance->refResources.push_back(framebuffer);
    }
    
    ShaderTableState& CommandList::GetShaderTableState(RT::IShaderTable *shaderTable)
    {
        auto* d3dShaderTable = Utility::CheckedCast<ShaderTable*>(shaderTable);
        if (d3dShaderTable->GetDesc().isCached)
            return d3dShaderTable->cacheState;

        if(auto it = m_ShaderTableStates.find(shaderTable); it != m_ShaderTableStates.end()){
            return *it->second;
        }

        m_ShaderTableStates[shaderTable] = std::make_unique<ShaderTableState>();

        return *m_ShaderTableStates[shaderTable];
    }
    
    void CommandList::BuildTopLevelAccelStructInternal(AccelStruct *accel, GpuVirtualAddress instanceDescsGpuVA, size_t numInstances, RT::AccelStructBuildFlags buildFlags)
    {
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geoDescs{};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs = GetAccelerationStructureBuildInputs(accel->GetDesc(), geoDescs);
        buildInputs.Flags = ConvertAccelerationStructureBuildFlags(buildFlags);
        buildInputs.NumDescs = static_cast<UINT>(numInstances);
        buildInputs.InstanceDescs = instanceDescsGpuVA;

        auto& context = m_Device.GetContext();
        if(context.device5 == nullptr)
            return;
        
        // 获取加速结构的构建信息
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
        context.device5->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

        const auto& dataBuffer = accel->dataBuffer;
        if(prebuildInfo.ResultDataMaxSizeInBytes > dataBuffer->GetDesc().byteSize){
            std::string msg = std::format("The buffer size of the top-level acceleration structure is too small. "
                "Required size: {}, actual size: {}", prebuildInfo.ResultDataMaxSizeInBytes, dataBuffer->GetDesc().byteSize);
            context.Error(msg);
            return;
        }

        // 创建需要的 scratch buffer
        bool performUpdate = HasFlags(buildFlags, RT::AccelStructBuildFlags::AllowUpdate);
        uint64_t scratchSize = performUpdate ? prebuildInfo.UpdateScratchDataSizeInBytes : prebuildInfo.ScratchDataSizeInBytes;
        auto scratchBuffer = AllocateGpuBuffer(scratchSize, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
        
        if(m_EnableAutomaticBarriers){
            m_StateTracker.RequireBufferState(dataBuffer, ResourceStates::AccelStructWrite);
        }
        CommitBarriers();

        // 创建加速结构
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
        buildDesc.DestAccelerationStructureData = dataBuffer->GetGpuVirtualAddress();
        buildDesc.Inputs = buildInputs;
        buildDesc.SourceAccelerationStructureData = performUpdate ? dataBuffer->GetGpuVirtualAddress() : 0;
        buildDesc.ScratchAccelerationStructureData = scratchBuffer.gpuAddress;
        m_CurrCmdList->cmdList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

        if(accel->GetDesc().trackLiveness){
            m_Instance->refResources.push_back(accel);
        }
    }
}

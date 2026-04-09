#include "StateTracking.h"

namespace DSM{
    static inline uint32_t CalculateSubresource(uint32_t mipLevel, uint32_t arraySlice, const TextureDesc& desc)
    {
        return mipLevel + arraySlice * desc.mipLevels;
    }

    void ResourceStateTracker::ConsumeBarriers(std::vector<TextureBarrier>& textureBarriers, std::vector<BufferBarrier>& bufferBarriers)
    {
        std::scoped_lock lock(m_TextureMutex, m_BufferMutex);
        textureBarriers = std::move(m_TextureBarriers);
        bufferBarriers = std::move(m_BufferBarriers);
        m_TextureBarriers.clear();
        m_BufferBarriers.clear();
    }

    TesxtureState *ResourceStateTracker::GetInternalTextureStateNoLock(ITexture *texture) const
    {
        TesxtureState* ret{};
        if(auto it = m_TextureStates.find(texture); it != m_TextureStates.end()){
            ret = it->second.get();
        }
        if(ret == nullptr){
            m_Callback->Message(MessageSeverity::Error, "Unknown prior state of texture");
            assert(ret != nullptr);
        }
        return ret;
    }
    
    BufferState *ResourceStateTracker::GetInternalBufferStateNoLock(IBuffer *buffer) const
    {
        BufferState* ret{};
        if(auto it = m_BufferStates.find(buffer); it != m_BufferStates.end()){
            ret = it->second.get();
        }
        if(ret == nullptr){
            m_Callback->Message(MessageSeverity::Error, "Unknown prior state of buffer");
            assert(ret != nullptr);
        }
        return ret;
    }

    void ResourceStateTracker::RegisterBuffer(IBuffer *buffer)
    {
        assert(buffer != nullptr);
        std::lock_guard lock(m_BufferMutex);
        if(!m_BufferStates.contains(buffer)){
            m_BufferStates[buffer] = std::make_unique<BufferState>();
            m_BufferStates[buffer]->state = buffer->GetDesc().initialState;
            // 单独处理需要保持初始状态的 Buffer
            if(const auto& desc = buffer->GetDesc(); desc.keepInitialState && !desc.isVolatile){
                m_KeepInitialStatesBuffers.push_back(buffer);
            }
        }
    }

    void ResourceStateTracker::RegisterTexture(ITexture *texture)
    {
        assert(texture != nullptr);
        std::lock_guard lock(m_TextureMutex);
        if(!m_TextureStates.contains(texture)){
            m_TextureStates[texture] = std::make_unique<TesxtureState>();
            m_TextureStates[texture]->state = texture->GetDesc().initialState;
            // 单独处理需要保持初始状态的 Texture
            if(const auto& desc = texture->GetDesc(); desc.keepInitialState){
                m_KeepInitialStatesTextures.push_back(texture);
            }
        }
    }

    void ResourceStateTracker::UnregisterBuffer(IBuffer *buffer)
    {
        assert(buffer != nullptr);
        std::lock_guard lock(m_BufferMutex);
        if(m_BufferStates.contains(buffer)){
            m_BufferStates.erase(buffer);
            if(const auto& desc = buffer->GetDesc(); desc.keepInitialState && !desc.isVolatile){
                std::erase(m_KeepInitialStatesBuffers, buffer);
            }
        }
    }

    void ResourceStateTracker::UnregisterTexture(ITexture *texture)
    {
        assert(texture != nullptr);
        std::lock_guard lock(m_TextureMutex);
        if(m_TextureStates.contains(texture)){
            m_TextureStates.erase(texture);
            if(const auto& desc = texture->GetDesc(); desc.keepInitialState){
                std::erase(m_KeepInitialStatesTextures, texture);
            }
        }
    }

    ResourceStates ResourceStateTracker::GetTextureSubresourceState(ITexture *texture, uint32_t mipLevel, uint32_t arraySlice)
    {
        const auto& desc = texture->GetDesc();
        std::lock_guard lock(m_TextureMutex);
        TesxtureState* texState = GetInternalTextureStateNoLock(texture);
        if(texState == nullptr){
            return desc.keepInitialState ? desc.initialState : ResourceStates::Unknown;
        }

        ResourceStates ret = texState->state;
        if(!texState->subresourceStates.empty()){
            ret = texState->subresourceStates[CalculateSubresource(mipLevel, arraySlice, desc)];
        }
        return ret;
    }
    
    ResourceStates ResourceStateTracker::GetBufferState(IBuffer *buffer)
    {
        std::lock_guard lock(m_BufferMutex);
        BufferState* bufferState = GetInternalBufferStateNoLock(buffer);
        if(bufferState == nullptr){
            return buffer->GetDesc().keepInitialState ? 
                buffer->GetDesc().initialState : ResourceStates::Unknown;
        }
        return bufferState->state;
    }
    
    void ResourceStateTracker::SetEnableUavBarrierForTexture(ITexture *texture, bool enable)
    {
        std::lock_guard lock(m_TextureMutex);
        TesxtureState* state = GetInternalTextureStateNoLock(texture);
        state->enableUavBarriers = enable;
        state->firstUavBarrierPlaced = false;
    }

    void ResourceStateTracker::SetEnableUavBarrierForBuffer(IBuffer * buffer, bool enable)
    {
        std::lock_guard lock(m_BufferMutex);
        BufferState* state = GetInternalBufferStateNoLock(buffer);
        state->enableUavBarriers = enable;
        state->firstUavBarrierPlaced = false;
    }
    
    void ResourceStateTracker::RequireTextureState(ITexture *texture, TextureSubresourceSet subresources, ResourceStates state)
    {
        std::lock_guard lock(m_TextureMutex);
        RequireTextureStateNoLock(texture, subresources, state);
    }

    void ResourceStateTracker::RequireTextureStateNoLock(ITexture *texture, TextureSubresourceSet subresources, ResourceStates state)
    {
        const auto& desc = texture->GetDesc();
        subresources = subresources.Resolve(desc, false);

        TesxtureState* texState = GetInternalTextureStateNoLock(texture);

        if(subresources.IsEntireTexture(desc) && texState->subresourceStates.empty()){
            bool needTransition = texState->state != state;
            // UAV Resource
            bool needUav = HasFlags(state, ResourceStates::UnorderedAccess) && 
                (texState->enableUavBarriers || !texState->firstUavBarrierPlaced);

            if(needTransition || needUav){
                TextureBarrier barrier{};
                barrier.texture = texture;
                barrier.entireTexture = true;
                barrier.stateBefore = texState->state;
                barrier.stateAfter = state;
                m_TextureBarriers.push_back(std::move(barrier));
            }

            texState->state = state;

            // 使用 UAV Barrier 在完成操作之前不可对 UAV 资源进行访问
            if(!needTransition && needUav){
                texState->firstUavBarrierPlaced = true;
            }
        }
        else{
            bool stateExpanded = false;
            if(texState->subresourceStates.empty()){    // 扩展子资源状态
                texState->subresourceStates.resize(desc.arraySize * desc.mipLevels, texState->state);
                stateExpanded = true;
            }

            // 遍历所有子资源
            for(uint32_t i = 0; i < subresources.numMipLevels; ++i){
                auto mipLevel = i + subresources.baseMipLevel;
                for(uint32_t j = 0; j < subresources.numArraySlices; ++j){
                    auto arraySlices = j + subresources.baseArraySlice;

                    uint32_t subresourceIndex = CalculateSubresource(mipLevel, arraySlices, desc);
                    ResourceStates& subresourceState = texState->subresourceStates[subresourceIndex];

                    bool needTransition = subresourceState != state;
                    bool needUav = HasFlags(state, ResourceStates::UnorderedAccess) && 
                        (texState->enableUavBarriers || !texState->firstUavBarrierPlaced);

                    if(needTransition || needUav){
                        TextureBarrier barrier{};
                        barrier.texture = texture;
                        barrier.entireTexture = false;
                        barrier.mipLevel = mipLevel;
                        barrier.arraySlice = arraySlices;
                        barrier.stateBefore = subresourceState;
                        barrier.stateAfter = state;
                        m_TextureBarriers.push_back(std::move(barrier));
                    }

                    subresourceState = state;

                    // UAV Barrier
                    if(!needTransition && needUav){
                        texState->firstUavBarrierPlaced = true;
                    }
                }
            }
        }
    }
    
    void ResourceStateTracker::RequireBufferState(IBuffer *buffer, ResourceStates state)
    {
        std::lock_guard lock(m_BufferMutex);
        RequireBufferStateNoLock(buffer, state);
    }

    void ResourceStateTracker::RequireBufferStateNoLock(IBuffer *buffer, ResourceStates state)
    {
        assert(buffer != nullptr);
        const auto& desc = buffer->GetDesc();
        
        // Cpu 可见的 Buffer 不可转换状态
        if(desc.cpuAccess != CpuAccessMode::None) return;

        BufferState* bufferState = GetInternalBufferStateNoLock(buffer);
        
        bool needTransition = bufferState->state != state;
        bool needUav = HasFlags(state, ResourceStates::UnorderedAccess) &&
            (bufferState->enableUavBarriers || !bufferState->firstUavBarrierPlaced);

        if(needTransition){
            auto it = std::find_if(m_BufferBarriers.begin(), m_BufferBarriers.end(), 
                [buffer](const BufferBarrier& barrier){
                    return barrier.buffer == buffer;
                });
            // 一个 Buffer 可能充当多种资源，因此将两个状态合并
            if(it != m_BufferBarriers.end()){
                it->stateAfter |= state;
                bufferState->state = it->stateAfter;
                return;
            }
        }

        if(needTransition || needUav){
            BufferBarrier barrier{};
            barrier.buffer = buffer;
            barrier.stateBefore = bufferState->state;
            barrier.stateAfter = state;
            m_BufferBarriers.push_back(std::move(barrier));
        }
        if(needUav && !needTransition){
            bufferState->firstUavBarrierPlaced = true;
        }
        bufferState->state = state;
    }

    void ResourceStateTracker::KeepTextureInitialStates()
    {
        std::lock_guard stateLock(m_TextureMutex);
        for(auto& texture : m_KeepInitialStatesTextures){
            RequireTextureStateNoLock(texture, AllSubresources, texture->GetDesc().initialState);
        }
    }

    void ResourceStateTracker::KeepBufferInitialStates()
    {
            std::lock_guard stateLock(m_BufferMutex);
        for(auto& buffer : m_KeepInitialStatesBuffers){
            RequireBufferStateNoLock(buffer, buffer->GetDesc().initialState);
        }
    }
}
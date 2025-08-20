#include "StateTracking.h"

namespace DSM{
    static inline uint32_t CalculateSubresource(uint32_t mipLevel, uint32_t arraySlice, const TextureDesc& desc)
    {
        return mipLevel + arraySlice * desc.mipLevels;
    }

    void ResourceStateTracker::ClearBarriers()
    {
        m_TextureBarriers.clear();
        m_BufferBarriers.clear();
    }

    TesxtureState *ResourceStateTracker::GetTextureState(ITexture *texture, bool allowCreate)
    {
        TesxtureState* ret{};
        if(auto it = m_TextureStates.find(texture); it != m_TextureStates.end()){
            ret = it->second.get();
        }
        else if(allowCreate) {
            auto texState = std::make_unique<TesxtureState>();
            texState->state = texture->GetDesc().initialState;
            ret = texState.get();
            m_TextureStates.emplace(texture, std::move(texState));
            if(texture->GetDesc().keepInitialState){
                ret->state = texture->GetDesc().initialState;
            }
        }
        return ret;
    }
    
    BufferState *ResourceStateTracker::GetBufferState(IBuffer *buffer, bool allowCreate)
    {
        BufferState* ret{};
        if(auto it = m_BufferStates.find(buffer); it != m_BufferStates.end()){
            ret = it->second.get();
        }
        else if(allowCreate) {
            auto bufferState = std::make_unique<BufferState>();
            bufferState->state = buffer->GetDesc().initialState;
            ret = bufferState.get();
            m_BufferStates.emplace(buffer, std::move(bufferState));
            if(buffer->GetDesc().keepInitialState){
                ret->state = buffer->GetDesc().initialState;
            }
        }
        return ret;
    }
    
    ResourceStates ResourceStateTracker::GetTextureSubresourceState(ITexture *texture, uint32_t mipLevel, uint32_t arraySlice)
    {
        const auto& desc = texture->GetDesc();
        TesxtureState* texState = GetTextureState(texture, false);
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
        BufferState* bufferState = GetBufferState(buffer, false);
        if(bufferState == nullptr){
            return buffer->GetDesc().keepInitialState ? 
                buffer->GetDesc().initialState : ResourceStates::Unknown;
        }
        return bufferState->state;
    }
    
    void ResourceStateTracker::SetEnableUavBarrierForTexture(ITexture *texture, bool enable)
    {
        TesxtureState* state = GetTextureState(texture, true);
        state->enableUavBarriers = enable;
        state->firstUavBarrierPlaced = false;
    }

    void ResourceStateTracker::SetEnableUavBarrierForBuffer(IBuffer * buffer, bool enable)
    {
        BufferState* state = GetBufferState(buffer, true);
        state->enableUavBarriers = enable;
        state->firstUavBarrierPlaced = false;
    }
    
    void ResourceStateTracker::RequireTextureState(ITexture *texture, TextureSubresourceSet subresources, ResourceStates state)
    {
        const auto& desc = texture->GetDesc();
        subresources = subresources.Resolve(desc, false);

        TesxtureState* texState = GetTextureState(texture, true);

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
            auto messageError = [this](const std::string& name){
                std::string msg = "Unknown prior state of texture " + std::string{DebugNameToString(name)};
                msg += ".Call CommandList::beginTrackingTextureState(...) before using the texture";
                msg += "or use the keepInitialState and initialState members of TextureDesc.";
                m_Callback->Message(MessageSeverity::Error, msg.c_str());
            };
            bool stateExpanded = false;
            if(texState->subresourceStates.empty()){    // 扩展子资源状态
                if(texState->state == ResourceStates::Unknown){
                    messageError(desc.debugName);
                }

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

                    if(subresourceState == ResourceStates::Unknown && !stateExpanded){
                        messageError(desc.debugName);
                    }

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
        assert(buffer != nullptr);
        const auto& desc = buffer->GetDesc();
        
        // Cpu 可见的 Buffer 不可转换状态
        if(desc.cpuAccess != CpuAccessMode::None) return;

        BufferState* bufferState = GetBufferState(buffer, true);
        if(bufferState->state == ResourceStates::Unknown){
            std::string msg = "Unknown prior state of texture " + std::string{DebugNameToString(desc.debugName)};
            msg += ".Call CommandList::beginTrackingTextureState(...) before using the texture";
            msg += "or use the keepInitialState and initialState members of TextureDesc.";
            m_Callback->Message(MessageSeverity::Error, msg.c_str());
        }

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
    
    void ResourceStateTracker::BeginTrackingTextureState(ITexture *texture, TextureSubresourceSet subresources)
    {
        const auto& desc = texture->GetDesc();
        subresources = subresources.Resolve(desc, false);

        TesxtureState* texState = GetTextureState(texture, true);
        if(subresources.IsEntireTexture(desc)){
            //texState->state = stateBits;
            texState->subresourceStates.clear();
        }
        else{
            if(texState->subresourceStates.empty()){
                texState->subresourceStates.resize(desc.mipLevels * desc.arraySize, texState->state);
            }
            texState->state = ResourceStates::Unknown;

            // // 初始化所有子状态
            // for(uint32_t i = 0; i < subresources.numMipLevels; ++i){
            //     uint32_t mipLevel = i + subresources.baseMipLevel;
            //     for(uint32_t j = 0; j < subresources.numArraySlices; ++j){
            //         uint32_t arraySlice = j + subresources.baseArraySlice;
            //         uint32_t subresourceIndex = CalculateSubresource(mipLevel, arraySlice, desc);
            //         texState->subresourceStates[subresourceIndex];
            //     }
            // }
        }
    }
    
    void ResourceStateTracker::BeginTrackingBufferState(IBuffer *buffer)
    {
        BufferState* bufferState = GetBufferState(buffer, true);
    }

    void ResourceStateTracker::KeepTextureInitialStates()
    {        
        for(auto& [tex, state] : m_TextureStates){
            if(auto& desc = tex->GetDesc(); desc.keepInitialState){
                RequireTextureState(tex, AllSubresources, desc.initialState);
            }
        }
    }

    void ResourceStateTracker::KeepBufferInitialStates()
    {
        for(auto& [buffer, state] : m_BufferStates){
            if(auto& desc = buffer->GetDesc(); 
                desc.keepInitialState && !desc.isVolatile){
                RequireBufferState(buffer, desc.initialState);
            }
        }
    }
    
    void ResourceStateTracker::CommandListSubmitted()
    {
        m_TextureBarriers.clear();
        m_BufferStates.clear();
    }
}
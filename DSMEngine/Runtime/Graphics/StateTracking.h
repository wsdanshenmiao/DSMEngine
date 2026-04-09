#pragma once
#ifndef __STATETRACKING_H__
#define __STATETRACKING_H__

#include "Runtime/Graphics/Texture.h"
#include "Runtime/Graphics/Buffer.h"
#include <mutex>
#include <unordered_map>


namespace DSM {
    struct TesxtureState 
    {
        std::vector<ResourceStates> subresourceStates{};
        ResourceStates state = ResourceStates::Unknown;
        bool enableUavBarriers = true;
        bool firstUavBarrierPlaced = false;
    };

    struct BufferState
    {
        ResourceStates state = ResourceStates::Unknown;
        bool enableUavBarriers = true;
        bool firstUavBarrierPlaced = false;
    };

    struct TextureBarrier
    {
        ITexture* texture{};
        uint32_t mipLevel = 0;
        uint32_t arraySlice = 0;
        bool entireTexture = false;
        ResourceStates stateBefore = ResourceStates::Unknown;
        ResourceStates stateAfter = ResourceStates::Unknown;
    };

    struct BufferBarrier
    {
        IBuffer* buffer{};
        ResourceStates stateBefore = ResourceStates::Unknown;
        ResourceStates stateAfter = ResourceStates::Unknown;
    };

    // 追踪资源状态的辅助类
    class ResourceStateTracker
    {
    public:
        ResourceStateTracker(IMessageCallback* callback) : m_Callback(callback) {}

        void RegisterBuffer(IBuffer* buffer);
        void RegisterTexture(ITexture* texture);
        void UnregisterBuffer(IBuffer* buffer);
        void UnregisterTexture(ITexture* texture);

        ResourceStates GetTextureSubresourceState(ITexture* texture, uint32_t mipLevel, uint32_t arraySlice);
        ResourceStates GetBufferState(IBuffer* buffer);

        void SetEnableUavBarrierForTexture(ITexture* texture, bool enable);
        void SetEnableUavBarrierForBuffer(IBuffer* buffer, bool enable);

        // 记录资源的状态转换
        void RequireTextureState(ITexture* texture, TextureSubresourceSet subresources, ResourceStates state);
        void RequireBufferState(IBuffer* buffer, ResourceStates state);
        
        void KeepTextureInitialStates();
        void KeepBufferInitialStates();

        void ConsumeBarriers(std::vector<TextureBarrier>& textureBarriers, std::vector<BufferBarrier>& bufferBarriers);

    private:
        TesxtureState* GetInternalTextureStateNoLock(ITexture* texture) const;
        BufferState* GetInternalBufferStateNoLock(IBuffer* buffer) const;
        void RequireTextureStateNoLock(ITexture* texture, TextureSubresourceSet subresources, ResourceStates state);
        void RequireBufferStateNoLock(IBuffer* buffer, ResourceStates state);

    private:
        IMessageCallback* m_Callback;

        mutable std::mutex m_TextureMutex{};
        mutable std::mutex m_BufferMutex{};

        // 记录各个资源的状态
        std::unordered_map<ITexture*, std::unique_ptr<TesxtureState>> m_TextureStates{};
        std::unordered_map<IBuffer*, std::unique_ptr<BufferState>> m_BufferStates{};

        std::vector<ITexture*> m_KeepInitialStatesTextures{};
        std::vector<IBuffer*> m_KeepInitialStatesBuffers{};

        std::vector<TextureBarrier> m_TextureBarriers{};
        std::vector<BufferBarrier> m_BufferBarriers{};
    };
} // namespace DSM 

#endif
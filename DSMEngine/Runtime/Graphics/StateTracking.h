#pragma once
#ifndef __STATETRACKING_H__
#define __STATETRACKING_H__

#include "Runtime/Graphics/Texture.h"
#include "Runtime/Graphics/Buffer.h"
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

        [[nodiscard]] const std::vector<TextureBarrier>& GetTextureBarriers() const { return m_TextureBarriers; }
        [[nodiscard]] const std::vector<BufferBarrier>& GetBufferBarriers() const { return m_BufferBarriers; }
        void ClearBarriers();

    private:
        TesxtureState* GetInternalTextureState(ITexture* texture);
        BufferState* GetInternalBufferState(IBuffer* buffer);

    private:
        IMessageCallback* m_Callback;

        // 记录各个资源的状态
        std::unordered_map<ITexture*, std::unique_ptr<TesxtureState>> m_TextureStates{};
        std::unordered_map<IBuffer*, std::unique_ptr<BufferState>> m_BufferStates{};

        std::vector<TextureBarrier> m_TextureBarriers{};
        std::vector<BufferBarrier> m_BufferBarriers{};
    };
} // namespace DSM 

#endif
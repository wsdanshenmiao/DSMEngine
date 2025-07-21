#pragma once
#ifndef __COMMANDLIST_H__
#define __COMMANDLIST_H__


namespace DSM{
    
    struct CommandListParameters
    {
        // 用于将数据上传到设备的内存块的最小大小。
        size_t uploadChunkSize = 64 * 1024;

        // 用于创建加速结构的最小大小
        size_t scratchChunkSize = 64 * 1024;

        // 用于创建加速结构的最大大小
        size_t scratchMaxMemory = 1024 * 1024 * 1024;

        // 命令队列的类型
        CommandQueue queueType = CommandQueue::Graphics;

        CommandListParameters& SetUploadChunkSize(size_t value) { uploadChunkSize = value; return *this; }
        CommandListParameters& SetScratchChunkSize(size_t value) { scratchChunkSize = value; return *this; }
        CommandListParameters& SetScratchMaxMemory(size_t value) { scratchMaxMemory = value; return *this; }
        CommandListParameters& SetQueueType(CommandQueue value) { queueType = value; return *this; }
    };

    class ICommandList : public IResource
    {
    public:
        virtual void Open() = 0;
        virtual void Close() = 0;

        virtual void ClearState() = 0;

        virtual void ClearTextureFloat(ITexture* t, TextureSubresourceSet subresources, const Color& clearColor) = 0;
        virtual void ClearTextureUInt(ITexture* t, TextureSubresourceSet subresources, uint32_t clearColor) = 0;
        virtual void ClearDepthStencilTexture(ITexture* t, TextureSubresourceSet subresources, bool clearDepth,
            float depth, bool clearStencil, uint8_t stencil) = 0;

        virtual void CopyTexture(ITexture* dest, const TextureSlice& destSlice, ITexture* src, const TextureSlice& srcSlice) = 0;
        virtual void CopyTexture(IStagingTexture* dest, const TextureSlice& destSlice, ITexture* src, const TextureSlice& srcSlice) = 0;
        virtual void CopyTexture(ITexture* dest, const TextureSlice& destSlice, IStagingTexture* src, const TextureSlice& srcSlice) = 0;
        virtual void WriteTexture(ITexture* dest, uint32_t arraySlice, uint32_t mipLevel, 
            const void* data, size_t rowPitch, size_t depthPitch = 0) = 0;
        // 将多重采样资源复制到非多重采样资源
        virtual void ResolveTexture(ITexture* dest, const TextureSubresourceSet& dstSubresources, 
            ITexture* src, const TextureSubresourceSet& srcSubresources) = 0;

        virtual void WriteBuffer(IBuffer* b, const void* data, size_t dataSize, uint64_t destOffsetBytes = 0) = 0;
        virtual void ClearBufferUInt(IBuffer* b, uint32_t clearValue) = 0;
        virtual void CopyBuffer(IBuffer* dest, uint64_t destOffsetBytes, 
            IBuffer* src, uint64_t srcOffsetBytes, uint64_t dataSizeBytes) = 0;

        //设置根常数
        virtual void SetPushConstants(const void* data, size_t byteSize) = 0;
        
        virtual void SetGraphicsState(const GraphicsState& state) = 0;
        virtual void Draw(const DrawArguments& args) = 0;
        virtual void DrawIndexed(const DrawArguments& args) = 0;
        virtual void DrawIndirect(uint32_t offsetBytes, uint32_t drawCount = 1) = 0;
        virtual void DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount = 1) = 0;
        
        virtual void SetComputeState(const ComputeState& state) = 0;
        virtual void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) = 0;
        virtual void DispatchIndirect(uint32_t offsetBytes) = 0;

        virtual void SetMeshletState(const MeshletState& state) = 0;
        virtual void DispatchMesh(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) = 0;

        // 查询GPU上的时间信息
        virtual void BeginTimerQuery(ITimerQuery* query) = 0;
        virtual void EndTimerQuery(ITimerQuery* query) = 0;

        virtual void BeginEvent(const char* name) = 0;
        virtual void EndEvent() = 0;

        virtual void setEnableAutomaticBarriers(bool enable) = 0;

        virtual void setResourceStatesForBindingSet(IBindingSet* bindingSet) = 0;
        
        void setResourceStatesForFramebuffer(IFramebuffer* framebuffer);

        virtual void setEnableUavBarriersForTexture(ITexture* texture, bool enableBarriers) = 0;

        virtual void setEnableUavBarriersForBuffer(IBuffer* buffer, bool enableBarriers) = 0;

        virtual void beginTrackingTextureState(ITexture* texture, TextureSubresourceSet subresources,
            ResourceStates stateBits) = 0;

        virtual void beginTrackingBufferState(IBuffer* buffer, ResourceStates stateBits) = 0;

        virtual void setTextureState(ITexture* texture, TextureSubresourceSet subresources,
            ResourceStates stateBits) = 0;

        virtual void setBufferState(IBuffer* buffer, ResourceStates stateBits) = 0;

        virtual void setAccelStructState(rt::IAccelStruct* as, ResourceStates stateBits) = 0;

        virtual void setPermanentTextureState(ITexture* texture, ResourceStates stateBits) = 0;

        virtual void setPermanentBufferState(IBuffer* buffer, ResourceStates stateBits) = 0;

        virtual void commitBarriers() = 0;

        virtual ResourceStates getTextureSubresourceState(ITexture* texture, ArraySlice arraySlice,
            MipLevel mipLevel) = 0;

        virtual ResourceStates getBufferState(IBuffer* buffer) = 0;

        virtual IDevice* GetDevice() = 0;

        virtual const CommandListParameters& GetDesc() = 0;
    };
}

#endif
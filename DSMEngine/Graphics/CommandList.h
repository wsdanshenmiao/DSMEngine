#pragma once
#ifndef __COMMANDLIST_H__
#define __COMMANDLIST_H__

#include "PipelineState.h"
#include "Buffer.h"

namespace DSM{
    struct IDevice;
    
    struct CommandListParameters
    {
        // 用于将数据上传到设备的内存块的最小大小。
        size_t uploadChunkSize = 64 * 1024;

        // 用于创建加速结构的最小大小
        size_t scratchChunkSize = 64 * 1024;

        // 命令队列的类型
        CommandQueueType queueType = CommandQueueType::Graphics;

        std::string debugName{};

        CommandListParameters& SetUploadChunkSize(size_t value) { uploadChunkSize = value; return *this; }
        CommandListParameters& SetScratchChunkSize(size_t value) { scratchChunkSize = value; return *this; }
        CommandListParameters& SetQueueType(CommandQueueType value) { queueType = value; return *this; }
        CommandListParameters& SetDebugName(const std::string& name) { debugName = name; return *this; }
    };

    struct ICommandList : public IResource
    {
        virtual void Open() = 0;
        virtual void Close() = 0;

        virtual void ClearState() = 0;

        virtual void ClearTextureFloat(ITexture* t, TextureSubresourceSet subresources, const Color& clearColor) = 0;
        virtual void ClearTextureUInt(ITexture* t, TextureSubresourceSet subresources, uint32_t clearColor) = 0;
        virtual void ClearDepthStencilTexture(ITexture* t, TextureSubresourceSet subresources, bool clearDepth,
            float depth, bool clearStencil, uint8_t stencil) = 0;

        virtual void CopyTexture(ITexture* _dest, TextureSlice destSlice, ITexture* _src, TextureSlice srcSlice) = 0;
        virtual void WriteTexture(ITexture* _dest, uint32_t arraySlice, uint32_t mipLevel, 
            const void* data, size_t rowPitch, size_t depthPitch = 0) = 0;
        // 将多重采样资源复制到非多重采样资源
        virtual void ResolveTexture(ITexture* _dest, TextureSubresourceSet dstSubresources, 
            ITexture* _src, TextureSubresourceSet srcSubresources) = 0;

        virtual void WriteBuffer(IBuffer* b, const void* data, size_t dataSize, uint64_t destOffsetBytes = 0) = 0;
        virtual void ClearBufferUInt(IBuffer* b, uint32_t clearValue) = 0;
        virtual void CopyBuffer(IBuffer* _dest, uint64_t destOffsetBytes, 
            IBuffer* _src, uint64_t srcOffsetBytes, uint64_t dataSizeBytes) = 0;

        //设置根常数
        virtual void SetPushConstants(const void* data, size_t byteSize) = 0;
        
        // 图形管线的调用
        virtual void SetGraphicsState(const GraphicsState& state) = 0;
        virtual void Draw(const DrawArguments& args) = 0;
        virtual void DrawIndexed(const DrawArguments& args) = 0;
        virtual void DrawIndirect(uint32_t offsetBytes, uint32_t drawCount = 1) = 0;
        virtual void DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount = 1) = 0;
        
        // 计算管线
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

        virtual void SetEnableAutomaticBarriers(bool enable) = 0;
        virtual void SetEnableUavBarriersForTexture(ITexture* texture, bool enableBarriers) = 0;
        virtual void SetEnableUavBarriersForBuffer(IBuffer* buffer, bool enableBarriers) = 0;
        
        virtual void SetTextureState(ITexture* texture, TextureSubresourceSet subresources, ResourceStates stateBits) = 0;
        virtual void SetBufferState(IBuffer* buffer, ResourceStates stateBits) = 0;
        virtual void SetResourceStatesForBindingSet(IBindingSet* bindingSet) = 0;
        void SetResourceStatesForFramebuffer(IFramebuffer* framebuffer)
        {
            const FramebufferDesc& desc = framebuffer->GetDesc();
            for(const auto& attachment : desc.colorAttachments){
                SetTextureState(attachment.texture, attachment.subresources, ResourceStates::RenderTarget);
            }

            auto& depth = desc.depthAttachment;
            if(depth.Valid()){
                SetTextureState(depth.texture, depth.subresources, 
                    depth.isReadOnly ? ResourceStates::DepthRead : ResourceStates::DepthWrite);
            }
        }

        virtual ResourceStates GetTextureSubresourceState(ITexture* texture, uint32_t arraySlice, uint32_t mipLevel) = 0;
        virtual ResourceStates GetBufferState(IBuffer* buffer) = 0;

        virtual void CommitBarriers() = 0;

        virtual IDevice* GetDevice() = 0;

        virtual const CommandListParameters& GetDesc() = 0;
    };
    using CommandListHandle = RefPtr<ICommandList>;
}

#endif
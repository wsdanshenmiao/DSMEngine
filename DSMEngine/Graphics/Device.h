#pragma once
#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "CommandList.h"


namespace DSM{
    
    struct IDevice : public IResource
    {
        virtual HeapHandle CreateHeap(const HeapDesc& d) = 0;

        virtual TextureHandle CreateTexture(const TextureDesc& d) = 0;
        virtual MemoryRequirements GetTextureMemoryRequirements(ITexture* texture) = 0;
        // 将保留资源绑定到具体的堆中
        virtual bool BindTextureMemory(ITexture* texture, IHeap* heap, uint64_t offset) = 0;

        virtual TextureHandle CreateHandleForNativeTexture(ObjectType objectType, Object texture, const TextureDesc& desc) = 0;

        virtual void GetTextureTiling(ITexture* texture, uint32_t* numTiles, PackedMipDesc* desc, TileShape* tileShape, uint32_t* subresourceTilingsNum, SubresourceTiling* subresourceTilings) = 0;
        virtual void UpdateTextureTileMappings(ITexture* texture, const TextureTilesMapping* tileMappings, uint32_t numTileMappings, CommandQueueType executionQueue = CommandQueueType::Graphics) = 0;

        virtual BufferHandle CreateBuffer(const BufferDesc& d) = 0;
        virtual void* MapBuffer(IBuffer* buffer, CpuAccessMode cpuAccess) = 0;
        virtual void UnmapBuffer(IBuffer* buffer) = 0;
        virtual MemoryRequirements GetBufferMemoryRequirements(IBuffer* buffer) = 0;
        // 将保留资源绑定到具体的堆中
        virtual bool BindBufferMemory(IBuffer* buffer, IHeap* heap, uint64_t offset) = 0;

        virtual BufferHandle CreateHandleForNativeBuffer(ObjectType objectType, Object buffer, const BufferDesc& desc) = 0;

        virtual ShaderHandle CreateShader(const ShaderDesc& d, const void* binary, size_t binarySize) = 0;
        virtual ShaderLibraryHandle CreateShaderLibrary(const void* binary, size_t binarySize) = 0;
        
        virtual SamplerHandle CreateSampler(const SamplerDesc& d) = 0;

        virtual InputLayoutHandle CreateInputLayout(const VertexAttributeDesc* d, uint32_t attributeCount, IShader* vertexShader) = 0;
        
        // 事件查询
        virtual EventQueryHandle CreateEventQuery() = 0;
        virtual void SetEventQuery(IEventQuery* query, CommandQueueType queue) = 0;
        virtual bool PollEventQuery(IEventQuery* query) = 0;
        virtual void WaitEventQuery(IEventQuery* query) = 0;
        virtual void ResetEventQuery(IEventQuery* query) = 0;

        // 时间查询
        virtual TimerQueryHandle CreateTimerQuery() = 0;
        virtual bool PollTimerQuery(ITimerQuery* query) = 0;
        virtual float GetTimerQueryTime(ITimerQuery* query) = 0;
        virtual void ResetTimerQuery(ITimerQuery* query) = 0;

        virtual GraphicsAPI GetGraphicsAPI() = 0;
        
        virtual FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) = 0;
        
        virtual GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, IFramebuffer* fb) = 0;
        
        virtual ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& desc) = 0;

        virtual MeshletPipelineHandle CreateMeshletPipeline(const MeshletPipelineDesc& desc, IFramebuffer* fb) = 0;

        virtual BindingLayoutHandle CreateBindingLayout(const BindingLayoutDesc& desc) = 0;
        virtual BindingLayoutHandle CreateBindlessLayout(const BindlessLayoutDesc& desc) = 0;

        virtual BindingSetHandle CreateBindingSet(const BindingSetDesc& desc, IBindingLayout* layout) = 0;
        virtual DescriptorTableHandle CreateDescriptorTable(IBindingLayout* layout) = 0;

        virtual void ResizeDescriptorTable(IDescriptorTable* descriptorTable, uint32_t newSize, bool keepContents = true) = 0;
        virtual bool WriteDescriptorTable(IDescriptorTable* descriptorTable, const BindingSetItem& item) = 0;

        virtual CommandListHandle CreateCommandList(const CommandListParameters& params = CommandListParameters()) = 0;
        virtual uint64_t ExecuteCommandLists(ICommandList* const* pCommandLists, size_t numCommandLists, CommandQueueType executionQueue = CommandQueueType::Graphics) = 0;
        virtual void QueueWaitForCommandList(CommandQueueType waitQueue, CommandQueueType executionQueue, uint64_t instance) = 0;
        // 等待成功返回true，遇到设备移除等问题返回false
        virtual bool WaitForIdle() = 0;

        virtual void RunGarbageCollection() = 0;

        // 检测特性支持
        virtual bool QueryFeatureSupport(Feature feature, void* pInfo = nullptr, size_t infoSize = 0) = 0;

        virtual FormatSupport QueryFormatSupport(Format format) = 0;

        virtual Object GetNativeQueue(ObjectType objectType, CommandQueueType queue) = 0;

        virtual IMessageCallback* GetMessageCallback() = 0;

        uint64_t ExecuteCommandList(ICommandList* commandList, CommandQueueType executionQueue = CommandQueueType::Graphics)
        {
            return ExecuteCommandLists(&commandList, 1, executionQueue);
        }
    };
    using DeviceHandle = RefPtr<IDevice>;
}

#endif
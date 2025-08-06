#pragma once
#ifndef __D3D12_DEVICE_H__
#define __D3D12_DEVICE_H__

#include "DescriptorHeap.h"
#include "D3D12Common.h"
#include <unordered_map>

namespace DSM::D3D12 {
    class RootSignature;
    
    class DeviceResources
    {
    public:
        explicit DeviceResources(const Context& context, const DeviceDesc& desc)
            :m_Context(context), renderTargetViewHeap(context), depthStencilViewHeap(context),
            shaderResourceViewHeap(context), samplerHeap(context) ,
            timerQueries(desc.maxTimerQueries, true) {}

    public:
        DescriptorHeap renderTargetViewHeap;
        DescriptorHeap depthStencilViewHeap;
        DescriptorHeap shaderResourceViewHeap;
        DescriptorHeap samplerHeap;
        Utility::BitSetAllocator timerQueries;

        // 根签名的缓存
        std::unordered_map<size_t, RootSignature*> rootsigCache;

    private:
        const Context& m_Context;
    };


    class EventQuery : public IEventQuery
    {
    public:
        RefPtr<ID3D12Fence> fence;
        uint64_t fenceCounter = 0;
        bool started = false;
        bool resolved = false;
    };

    class TimerQuery : public ITimerQuery
    {
    public:
        uint32_t beginQueryIndex = 0;
        uint32_t endQueryIndex = 0;

        RefPtr<ID3D12Fence> fence;
        uint64_t fenceCounter = 0;

        bool started = false;
        bool resolved = false;
        float time = 0.f;

        TimerQuery(DeviceResources& resources)
            : m_Resources(resources) { }

        ~TimerQuery() override
        {
            m_Resources.timerQueries.Release(beginQueryIndex / 2);
        }

    private:
        DeviceResources& m_Resources;
    };

    class Device final : public IDevice
    {
    public:
        HeapHandle CreateHeap(const HeapDesc& d) override;

        TextureHandle CreateTexture(const TextureDesc& desc) override;
        MemoryRequirements GetTextureMemoryRequirements(ITexture* texture) override;
        // 将保留资源绑定到具体的堆中
        bool BindTextureMemory(ITexture* _texture, IHeap* _heap, uint64_t offset) override;

        // 为原始的 D3D 资源创建纹理句柄
        TextureHandle CreateHandleForNativeTexture(ObjectType objectType, Object texture, const TextureDesc& desc) override;

        void GetTextureTiling(ITexture* texture, uint32_t* numTiles, PackedMipDesc* desc, TileShape* tileShape, uint32_t* subresourceTilingsNum, SubresourceTiling* subresourceTilings) override;
        void UpdateTextureTileMappings(ITexture* texture, const TextureTilesMapping* tileMappings, uint32_t numTileMappings, CommandQueue executionQueue = CommandQueue::Graphics) override;

        BufferHandle CreateBuffer(const BufferDesc& d) override;
        void* MapBuffer(IBuffer* _buffer, CpuAccessMode cpuAccess) override;
        void UnmapBuffer(IBuffer* _buffer) override;
        MemoryRequirements GetBufferMemoryRequirements(IBuffer* _buffer) override;
        // 将保留资源绑定到具体的堆中
        bool BindBufferMemory(IBuffer* _buffer, IHeap* _heap, uint64_t offset) override;

        BufferHandle CreateHandleForNativeBuffer(ObjectType objectType, Object _buffer, const BufferDesc& desc) override;

        ShaderHandle CreateShader(const ShaderDesc& d, const void* binary, size_t binarySize) override;
        ShaderLibraryHandle CreateShaderLibrary(const void* binary, size_t binarySize) override;
        
        SamplerHandle CreateSampler(const SamplerDesc& d) override;

        InputLayoutHandle CreateInputLayout(const VertexAttributeDesc* d, uint32_t attributeCount, IShader* vertexShader) override;
        
        // 事件查询
        EventQueryHandle CreateEventQuery() override;
        void SetEventQuery(IEventQuery* query, CommandQueue queue) override;
        bool PollEventQuery(IEventQuery* query) override;
        void WaitEventQuery(IEventQuery* query) override;
        void ResetEventQuery(IEventQuery* query) override;

        // 时间查询
        TimerQueryHandle CreateTimerQuery() override;
        bool PollTimerQuery(ITimerQuery* query) override;
        float GetTimerQueryTime(ITimerQuery* query) override;
        void ResetTimerQuery(ITimerQuery* query) override;

        GraphicsAPI GetGraphicsAPI() override;
        
        FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc) override;
        
        GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, IFramebuffer* fb) override;
        
        ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc& desc) override;

        MeshletPipelineHandle CreateMeshletPipeline(const MeshletPipelineDesc& desc, IFramebuffer* fb) override;

        BindingLayoutHandle CreateBindingLayout(const BindingLayoutDesc& desc) override;
        BindingLayoutHandle CreateBindlessLayout(const BindlessLayoutDesc& desc) override;

        BindingSetHandle CreateBindingSet(const BindingSetDesc& desc, IBindingLayout* layout) override;
        DescriptorTableHandle CreateDescriptorTable(IBindingLayout* layout) override;

        void ResizeDescriptorTable(IDescriptorTable* _descriptorTable, uint32_t newSize, bool keepContents = true) override;
        bool WriteDescriptorTable(IDescriptorTable* _descriptorTable, const BindingSetItem& item) override;

        //CommandListHandle CreateCommandList(const CommandListParameters& params = CommandListParameters()) override;
        //uint64_t ExecuteCommandLists(ICommandList* const* pCommandLists, size_t numCommandLists, CommandQueue executionQueue = CommandQueue::Graphics) override;
        void QueueWaitForCommandList(CommandQueue waitQueue, CommandQueue executionQueue, uint64_t instance) override;
        // 等待成功返回true，遇到设备移除等问题返回false
        bool WaitForIdle() override;

        // 检测特性支持
        bool QueryFeatureSupport(Feature feature, void* pInfo = nullptr, size_t infoSize = 0) override;

        FormatSupport QueryFormatSupport(Format format) override;

        Object GetNativeQueue(ObjectType objectType, CommandQueue queue) override;

        IMessageCallback* GetMessageCallback() override;

    private:
        Context m_Context;
        DeviceResources m_Resources;

        HANDLE m_FenceEvent;

        D3D12_FEATURE_DATA_D3D12_OPTIONS m_Options{};
    };
} // namespace DSM 

#endif
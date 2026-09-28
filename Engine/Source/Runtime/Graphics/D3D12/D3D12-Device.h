#pragma once
#ifndef __D3D12_DEVICE_H__
#define __D3D12_DEVICE_H__

#include "DescriptorHeap.h"
#include "D3D12Common.h"
#include "Runtime/Math/MathCommon.h"
#include <unordered_map>
#include <span>
#include <queue>
#include <mutex>

namespace DSM::D3D12 {
    class RootSignature;
    struct CommandListInstance;
    class Device;
    
    class DeviceResources
    {
    public:
        explicit DeviceResources(const Context& context, const DeviceDesc& desc);
        
        uint8_t GetFormatPlaneCount(DXGI_FORMAT format);

        void AddRootSignature(size_t hash, RootSignature* rootSig);
        void RemoveRootSignature(size_t hash);
        RootSignature* GetRootSignature(size_t hash);

    public:
        DescriptorHeap renderTargetViewHeap;
        DescriptorHeap depthStencilViewHeap;
        DescriptorHeap shaderResourceViewHeap;
        DescriptorHeap samplerHeap;
        Utility::BitSetAllocator timerQueries;

    private:
        const Context& m_Context;
        
        // 根签名的缓存
        std::unordered_map<size_t, RootSignature*> m_RootsigCache;
        std::mutex m_RootsigCacheMutex;

        // 不同类型的平面切片数
        std::unordered_map<DXGI_FORMAT, uint8_t> m_DxgiFormatPlaneCounts;
    };

    class CommandQueue
    {
    public:
        explicit CommandQueue(Device& device, CommandQueueType queueType);

        // 增加栅栏值
        uint64_t IncrementFence();
        bool IsFenceComplete(uint64_t fenceValue);
        // GPU 进行等待
        void StallForFence(uint64_t fenceValue);
        void StallForProducer(CommandQueue& producer);
        // CPU 进行等待
        void WaitForFence(uint64_t fenceValue);
        void WaitForIdle() { WaitForFence(IncrementFence()); }

        ID3D12CommandQueue* GetCommandQueue() const {return m_CommandQueue.Get();}
        ID3D12Fence* GetFence() const { return m_Fence.Get(); }
        uint64_t GetNextFenceValue() {return m_NextFenceValue;}

        uint64_t ExecuteCommandList(std::span<DSM::ICommandList* const> cmdLists);

        void ClearCompletedCmdList();

    protected:
        Device& m_Device;
        const CommandQueueType m_QueueType;
        RefPtr<ID3D12CommandQueue> m_CommandQueue{};

        // 用于 CPU 与 GPU 同步的栅栏
        RefPtr<ID3D12Fence> m_Fence{};
        std::uint64_t m_NextFenceValue = 1;
        std::uint64_t m_LastCompletedFenceValue = 0;
        std::mutex m_FenceMutex{};

        HANDLE m_FenceEventHandle{};
        std::mutex m_EventMutex{};
        
        std::atomic<uint64_t> m_RecordingInstance = 1;    // 命令提交次数
        // 提交命令列表后返回的实例，命令完成后才可释放
        std::queue<std::shared_ptr<CommandListInstance>> m_ActiveCmdLists{};
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

        TimerQuery(std::shared_ptr<DeviceResources> resources)
            : m_Resources(resources) { }

        ~TimerQuery() override
        {
            if(auto resources = m_Resources.lock()){
                resources->timerQueries.Release(beginQueryIndex / 2);
            }
        }

    private:
        std::weak_ptr<DeviceResources> m_Resources;
    };

    class Device final : public IDevice
    {
    public:
        explicit Device(DeviceDesc desc);
        virtual ~Device() override;

        Object GetNativeObject(ObjectType type) override;

        HeapHandle CreateHeap(const HeapDesc& d) override;

        TextureHandle CreateTexture(const TextureDesc& desc) override;
        MemoryRequirements GetTextureMemoryRequirements(ITexture* _texture) override;
        // 将保留资源绑定到具体的堆中
        bool BindTextureMemory(ITexture* _texture, IHeap* _heap, uint64_t offset) override;

        // 为原始的 D3D 资源创建纹理句柄
        TextureHandle CreateHandleForNativeTexture(ObjectType objectType, Object _texture, const TextureDesc& desc) override;

        void GetTextureTiling(ITexture* _texture, uint32_t* numTiles, PackedMipDesc* desc, TileShape* tileShape, uint32_t* subresourceTilingsNum, SubresourceTiling* subresourceTilings) override;
        void UpdateTextureTileMappings(ITexture* _texture, const TextureTilesMapping* tileMappings, uint32_t numTileMappings, CommandQueueType executionQueue = CommandQueueType::Graphics) override;

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

        InputLayoutHandle CreateInputLayout(std::span<const VertexAttributeDesc> descs, IShader* vertexShader) override;
        
        // 事件查询
        EventQueryHandle CreateEventQuery() override;
        void SetEventQuery(IEventQuery* query, CommandQueueType queue) override;
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
        RT::PipelineHandle CreateRayTracingPipeline(const RT::PipelineDesc& desc) override;

        // 光线追踪（DXR）
        RT::AccelStructHandle CreateAccelStruct(const RT::AccelStructDesc& desc) override;
        MemoryRequirements GetAccelStructMemoryRequirements(RT::IAccelStruct* as) override;
        bool BindAccelStructMemory(RT::IAccelStruct* as, IHeap* heap, uint64_t offset) override;

        BindingLayoutHandle CreateBindingLayout(const BindingLayoutDesc& desc) override;
        BindingLayoutHandle CreateBindlessLayout(const BindlessLayoutDesc& desc) override;

        BindingSetHandle CreateBindingSet(const BindingSetDesc& desc, IBindingLayout* layout) override;
        DescriptorTableHandle CreateDescriptorTable(IBindingLayout* layout) override;

        void ResizeDescriptorTable(IDescriptorTable* _descriptorTable, uint32_t newSize, bool keepContents = true) override;
        bool WriteDescriptorTable(IDescriptorTable* _descriptorTable, const BindingSetItem& item) override;

        DSM::CommandListHandle CreateCommandList(const CommandListParameters& params = CommandListParameters()) override;
        uint64_t ExecuteCommandLists(std::span<DSM::ICommandList* const> cmdLists, CommandQueueType executionQueue = CommandQueueType::Graphics) override;
        void QueueWaitForCommandList(CommandQueueType waitQueue, CommandQueueType executionQueue, uint64_t instance) override;
        // 等待成功返回true，遇到设备移除等问题返回false
        bool WaitForIdle() override;
        Object GetNativeQueue(ObjectType objectType, CommandQueueType queue) override;

        // 检测特性支持
        bool QueryFeatureSupport(Feature feature, void* pInfo = nullptr, size_t infoSize = 0) override;

        FormatSupport QueryFormatSupport(Format format) override;

        void RunGarbageCollection() override;
        
        IMessageCallback* GetMessageCallback() override;

        RootSignatureHandle BuildRootSignature(
            const BindingLayoutVector& pipelineLayouts, 
            bool allowInputLayout, bool isLocal, 
            const D3D12_ROOT_PARAMETER1* pCustomParameters = nullptr, 
            uint32_t numCustomParameters = 0) override;

        GraphicsPipelineHandle CreateHandleForNativeGraphicsPipeline(
            IRootSignature* rootSignature, 
            ID3D12PipelineState* pipelineState, 
            const GraphicsPipelineDesc& desc, 
            const FramebufferInfo& framebufferInfo) override;
        
        MeshletPipelineHandle CreateHandleForNativeMeshletPipeline(
            IRootSignature* rootSignature, 
            ID3D12PipelineState* pipelineState, 
            const MeshletPipelineDesc& desc, 
            const FramebufferInfo& framebufferInfo) override;

        DescriptorHeapHandle CreateDescriptorHeap(DescriptorHeapType type, uint32_t count, bool shaderVisible) override;

        IDescriptorHeap* GetDescriptorHeap(DescriptorHeapType heapType) override;

        CommandQueue* GetQueue(CommandQueueType type) { return m_CommandQueues[size_t(type)].get(); }
        Context& GetContext() noexcept { return m_Context; }
        RefPtr<RootSignature> GetRootSignature(const BindingLayoutVector& pipelineLayouts, bool allowInputLayout);

    private:
        using BindingLayoutVector = StaticVector<BindingLayoutHandle, c_MaxBindingLayouts>;

    private:
        const DeviceDesc m_Desc;
        Context m_Context;
        std::shared_ptr<DeviceResources> m_Resources;

        std::array<std::unique_ptr<CommandQueue>, (size_t)CommandQueueType::Count> m_CommandQueues;

        HANDLE m_FenceEvent;
        std::mutex m_Mutex;

        bool m_SinglePassStereoSupported = false;
        bool m_HlslExtensionsSupported = false;
        bool m_FastGeometryShaderSupported = false;
        bool m_RayTracingSupported = false;
        bool m_TraceRayInlineSupported = false;
        bool m_MeshletsSupported = false;
        bool m_VariableRateShadingSupported = false;
        bool m_OpacityMicromapSupported = false;
        bool m_RayTracingClustersSupported = false;
        bool m_LinearSweptSpheresSupported = false;
        bool m_SpheresSupported = false;
        bool m_ShaderExecutionReorderingSupported = false;
        bool m_SamplerFeedbackSupported = false;
        bool m_AftermathEnabled = false;
        bool m_HeapDirectlyIndexedEnabled = false;
        bool m_CoopVecInferencingSupported = false;
        bool m_CoopVecTrainingSupported = false;

        D3D12_FEATURE_DATA_D3D12_OPTIONS  m_Options = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 m_Options1 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 m_Options5 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS6 m_Options6 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 m_Options7 = {};
    };



} // namespace DSM 

#endif
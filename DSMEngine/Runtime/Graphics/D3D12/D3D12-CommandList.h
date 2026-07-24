#pragma once
#ifndef __D3D12_COMMANDLIST_H__
#define __D3D12_COMMANDLIST_H__

#include <mutex>
#include <set>
#include <unordered_map>
#include "Runtime/Graphics/D3D12.h"
#include "DynamicResourceAllocator.h"
#include "D3D12Common.h"

template <>
struct std::hash<std::pair<DSM::IBuffer*, uint64_t>> 
{
    size_t operator()(const std::pair<DSM::IBuffer*, uint64_t> &p) const noexcept 
    {
        // 将指针转换为整数进行哈希
        auto ptr_hash = std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(p.first));
        auto int_hash = std::hash<uint64_t>{}(p.second);
        
        const size_t kMul = 0x9ddfea08eb382d69ULL;
        
        size_t seed = ptr_hash;
        seed ^= int_hash + kMul + (seed << 6) + (seed >> 2);
        
        // 最终混合
        seed *= kMul;
        seed ^= seed >> 47;
        seed *= kMul;
        
        return seed;
    }
};

namespace DSM::D3D12 {
    struct Context;
    struct DynamicResourceLocation;
    class DeviceResources;
    class CommandQueue;
    class Device;
    class Buffer;
    class RootSignature;
    class TimerQuery;
    class Framebuffer;

    class InternalCommandList
    {
    public:
        static InternalCommandList* RequireCommandList(Device& device, const CommandListParameters& desc);
        static bool ReleaseCommandList(InternalCommandList* cmdList);
        static void Cleanup();

    private:
        InternalCommandList(Device& device, const CommandListParameters& desc);
        InternalCommandList(const InternalCommandList&) = delete;

    public:
        const CommandQueueType type;
        RefPtr<ID3D12CommandAllocator> allocator{};
        RefPtr<ID3D12GraphicsCommandList> cmdList{};
        RefPtr<ID3D12GraphicsCommandList4> cmdList4{};
        RefPtr<ID3D12GraphicsCommandList6> cmdList6{};

        uint64_t lastSubmittedFenceValue{};

        std::unique_ptr<DynamicResourceAllocator> uploadBufferAllocator{};
        std::unique_ptr<DynamicResourceAllocator> gpuBufferAllocator{};

    private:
        using FencevalueAndListPairQueue = std::queue<std::pair<uint64_t, InternalCommandList*>>;
        // 命令列表的缓存
        inline static std::set<std::unique_ptr<InternalCommandList>> sm_CmdListPool{};
        inline static std::array<FencevalueAndListPairQueue, (size_t)CommandQueueType::Count> sm_RetiredCmdLists{};
        inline static std::array<std::queue<InternalCommandList*>, (size_t)CommandQueueType::Count> sm_AvailableCmdLists{};
        inline static std::mutex sm_Mutex{};
    };

    struct CommandListInstance
    {
        RefPtr<ID3D12Fence> fence{};
        uint64_t submitFenceValue{};
        CommandQueueType queueType = CommandQueueType::Graphics; 
        RefPtr<ID3D12CommandAllocator> allocator{};
        RefPtr<ID3D12CommandList> cmdList{};

        // 引用资源，命令完成后才可释放
        std::vector<ResourceHandle> refResources;
        std::vector<RefPtr<IUnknown>> refNativeResources;
        std::vector<RefPtr<Buffer>> refBuffer;
        std::vector<RefPtr<TimerQuery>> refTimerQuery;
    };

    class CommandList : public ICommandList
    {
    public:
        CommandList(Device& device, std::shared_ptr<DeviceResources> resources, CommandListParameters desc);

        Object GetNativeObject(ObjectType type) override;

        void Open() override;
        void Close() override;

        void ClearState() override;

        void ClearTextureFloat(ITexture* t, TextureSubresourceSet subresources, const Color& clearColor) override;
        void ClearTextureUInt(ITexture* t, TextureSubresourceSet subresources, uint32_t clearColor) override;
        void ClearDepthStencilTexture(ITexture* t, TextureSubresourceSet subresources, bool clearDepth,
            float depth, bool clearStencil, uint8_t stencil) override;

        void CopyTexture(ITexture* _dest, TextureSlice destSlice, ITexture* _src, TextureSlice srcSlice) override;
        void WriteTexture(ITexture* _dest, uint32_t arraySlice, uint32_t mipLevel, 
            const void* data, size_t rowPitch, size_t depthPitch = 0) override;
        // 将多重采样资源复制到非多重采样资源
        void ResolveTexture(ITexture* _dest, TextureSubresourceSet dstSubresources, 
            ITexture* _src, TextureSubresourceSet srcSubresources) override;

        void CopyBuffer(IBuffer* _dest, uint64_t destOffsetBytes, 
            IBuffer* _src, uint64_t srcOffsetBytes, uint64_t dataSizeBytes) override;
        void WriteBuffer(IBuffer* b, const void* data, size_t dataSize, uint64_t destOffsetBytes = 0) override;
        void ClearBufferUInt(IBuffer* b, uint32_t clearValue) override;

        //设置根常数
        void SetPushConstants(const void* data, size_t byteSize) override;
        
        // 图形管线的调用
        void SetGraphicsState(const GraphicsState& state) override;
        void Draw(const DrawArguments& args) override;
        void DrawIndexed(const DrawArguments& args) override;
        void DrawIndirect(uint32_t offsetBytes, uint32_t drawCount = 1) override;
        void DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount = 1) override;
        
        // 计算管线
        void SetComputeState(const ComputeState& state) override;
        void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) override;
        void DispatchIndirect(uint32_t offsetBytes) override;

        void SetMeshletState(const MeshletState& state) override;
        void DispatchMesh(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) override;

        // 光线追踪（DXR）
        void SetRayTracingState(const RT::State& state) override;
        void DispatchRays(const RT::DispatchRaysArguments& args) override;
        void BuildBottomLevelAccelStruct(RT::IAccelStruct* as, const RT::GeometryDesc* geometries, size_t numGeometries, RT::AccelStructBuildFlags buildFlags) override;
        void BuildTopLevelAccelStruct(RT::IAccelStruct* as, const RT::InstanceDesc* instances, size_t numInstances, RT::AccelStructBuildFlags buildFlags) override;
        void CopyAccelStruct(RT::IAccelStruct* destination, RT::IAccelStruct* source) override;

        // 查询GPU上的时间信息
        void BeginTimerQuery(ITimerQuery* query) override;
        void EndTimerQuery(ITimerQuery* query) override;

        void BeginEvent(const char* name) override;
        void EndEvent() override;

        
        // Resource State
        void SetEnableAutomaticBarriers(bool enable) override;
        void SetEnableUavBarriersForTexture(ITexture* texture, bool enableBarriers) override;
        void SetEnableUavBarriersForBuffer(IBuffer* b, bool enableBarriers) override;

        void SetTextureState(ITexture* texture, TextureSubresourceSet subresources, ResourceStates stateBits) override;
        void SetBufferState(IBuffer* b, ResourceStates stateBits) override;
        void SetResourceStatesForBindingSet(IBindingSet* bindingSet) override;

        ResourceStates GetTextureSubresourceState(ITexture* texture, uint32_t arraySlice, uint32_t mipLevel) override;
        ResourceStates GetBufferState(IBuffer* b) override;

        void CommitBarriers() override;


        IDevice* GetDevice() override;

        const CommandListParameters& GetDesc() override { return m_Desc; }

                
        DynamicResourceLocation AllocateUploadBuffer(size_t size) override;
        DynamicResourceLocation AllocateGpuBuffer(size_t size) override;
        bool CommitDescriptorHeaps() override;
        D3D12_GPU_VIRTUAL_ADDRESS GetBufferGpuVA(IBuffer* b, uint64_t offset = 0) override;

        void UpdateGraphicsVolatileBuffers() override;
        void UpdateComputeVolatileBuffers() override;


        void SetResourceBindings(
            const BindingSetVector& bindings, 
            uint32_t bindingUpdateMask,
            IBuffer* indirectParams, 
            bool updateIndirectParams,
            const RootSignature* rootSignature,
            bool isGraphics);

        // 提交命令列表时调用
        std::shared_ptr<CommandListInstance> Executed(CommandQueue& queue);

    private:
        void ClearStateCache();

        void UpdateFramebuffer(Framebuffer* fb);

    private:
        struct VolatileBufferBinding
        {
            uint32_t rootParaIndex = 0;
            IBuffer* buffer = nullptr;
            uint64_t offset = 0;
            D3D12_GPU_VIRTUAL_ADDRESS address{};
        };
        
        Device& m_Device;
        std::weak_ptr<DeviceResources> m_Resources;
        ResourceStateTracker& m_StateTracker;

        // Command list
        CommandQueue* m_Queue;
        const CommandListParameters m_Desc;
        std::shared_ptr<CommandListInstance> m_Instance{};
        InternalCommandList* m_CurrCmdList = nullptr;

        // Cache for internal state
        GraphicsState m_CurrGraphicsState{};
        ComputeState m_CurrComputeState{};
        MeshletState m_CurrMeshletState{};
        bool m_CurrGraphicsStateValid = false;
        bool m_CurrComputeStateValid = false;
        bool m_CurrMeshletStateValid = false;

        // 光线追踪状态缓存
        RT::State m_CurrRayTracingState{};
        bool m_CurrRayTracingStateValid = false;
        D3D12_DISPATCH_RAYS_DESC m_DispatchRaysDesc{};

        ID3D12DescriptorHeap* m_CurrSRVHeap = nullptr;
        ID3D12DescriptorHeap* m_CurrSamplerHeap = nullptr;

        // 存放常量缓冲区的临时 Buffer
        StaticVector<VolatileBufferBinding, c_MaxVolatileConstantBuffers> m_GraphicsVolatileBuffers;
        StaticVector<VolatileBufferBinding, c_MaxVolatileConstantBuffers> m_ComputeVolatileBuffers;

        std::unordered_map<std::pair<IBuffer*, uint64_t>, D3D12_GPU_VIRTUAL_ADDRESS> m_VolatileBufferAddresses{};

        bool m_HasVolatileBufferWrites = false;
        bool m_EnableAutomaticBarriers = true;
    };

} // namespace DSM::D3D12 




#endif
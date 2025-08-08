#pragma once
#ifndef __D3D12_COMMANDLIST_H__
#define __D3D12_COMMANDLIST_H__

#include "Graphics/D3D12.h"

namespace DSM::D3D12 {
    struct Context;
    class DeviceResource;
    class CommandQueue;

    struct InternalCommandList
    {
        RefPtr<ID3D12CommandAllocator> allocator{};
        RefPtr<ID3D12GraphicsCommandList> cmdList{};
        RefPtr<ID3D12GraphicsCommandList4> cmdList4{};
        RefPtr<ID3D12GraphicsCommandList6> cmdList6{};

        uint64_t lastSubmittedInstance{};
    };

    struct CommandListInstance
    {
        RefPtr<ID3D12Fence> fence{};
        uint64_t submitFenceValue{};
        CommandQueueType queueType = CommandQueueType::Graphics; 
        RefPtr<ID3D12CommandAllocator> allocator{};
        RefPtr<ID3D12CommandList> cmdList{};

        // 引用资源，命令完成后才可释放
        std::vector<ResourceHandle> referenceResources;
        std::vector<RefPtr<IUnknown>> referenceNativeResources;
        std::vector<BufferHandle> referenceBuffer;
        std::vector<TimerQueryHandle> referenceTimerQuery;
    };

    class CommandList : public ICommandList
    {
    public:
        Object GetNativeObject(ObjectType type) override;

        void Open() override;
        void Close() override;

        void ClearState() override;

        void ClearTextureFloat(ITexture* t, TextureSubresourceSet subresources, const Color& clearColor) override;
        void ClearTextureUInt(ITexture* t, TextureSubresourceSet subresources, uint32_t clearColor) override;
        void ClearDepthStencilTexture(ITexture* t, TextureSubresourceSet subresources, bool clearDepth,
            float depth, bool clearStencil, uint8_t stencil) override;

        void CopyTexture(ITexture* dest, const TextureSlice& destSlice, ITexture* src, const TextureSlice& srcSlice) override;
        void WriteTexture(ITexture* dest, uint32_t arraySlice, uint32_t mipLevel, 
            const void* data, size_t rowPitch, size_t depthPitch = 0) override;
        // 将多重采样资源复制到非多重采样资源
        void ResolveTexture(ITexture* dest, const TextureSubresourceSet& dstSubresources, 
            ITexture* src, const TextureSubresourceSet& srcSubresources) override;

        void WriteBuffer(IBuffer* b, const void* data, size_t dataSize, uint64_t destOffsetBytes = 0) override;
        void ClearBufferUInt(IBuffer* b, uint32_t clearValue) override;
        void CopyBuffer(IBuffer* dest, uint64_t destOffsetBytes, 
            IBuffer* src, uint64_t srcOffsetBytes, uint64_t dataSizeBytes) override;

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

        // 查询GPU上的时间信息
        void BeginTimerQuery(ITimerQuery* query) override;
        void EndTimerQuery(ITimerQuery* query) override;

        void BeginEvent(const char* name) override;
        void EndEvent() override;

        void SetEnableAutomaticBarriers(bool enable) override;

        void SetEnableUavBarriersForTexture(ITexture* texture, bool enableBarriers) override;

        void SetEnableUavBarriersForBuffer(IBuffer* buffer, bool enableBarriers) override;

        void SetTextureState(ITexture* texture, TextureSubresourceSet subresources, ResourceStates stateBits) override;
        void SetBufferState(IBuffer* buffer, ResourceStates stateBits) override;
        void SetResourceStatesForBindingSet(IBindingSet* bindingSet) override;

        void CommitBarriers() override;

        ResourceStates GetTextureSubresourceState(ITexture* texture, uint32_t arraySlice, uint32_t mipLevel) override;

        ResourceStates GetBufferState(IBuffer* buffer) override;

        IDevice* GetDevice() override;

        const CommandListParameters& GetDesc() override;

                
        bool AllocateUploadBuffer(size_t size, void** pCpuAddress, D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress) override;
        bool CommitDescriptorHeaps() override;
        D3D12_GPU_VIRTUAL_ADDRESS GetBufferGpuVA(IBuffer* buffer) override;

        void UpdateGraphicsVolatileBuffers() override;
        void UpdateComputeVolatileBuffers() override;


        static void Cleanup();

    private:
        void ClearStateCache();


        static InternalCommandList* RequireCommandList(const Context& context, CommandQueueType type); 
        static void ReleaseCommandList(InternalCommandList* cmdList);

    private:
        static std::vector<std::unique_ptr<InternalCommandList>> sm_CommandListPool;
        
        const Context& m_Context;
        DeviceResource& m_Resources;
        const CommandListParameters m_Desc;
      
        CommandQueue* m_Queue;
        std::shared_ptr<CommandListInstance> m_Instance{};
        InternalCommandList* m_CurrCmdList = nullptr;

        // Cache for internal state

        GraphicsState m_CurrGraphicsState{};
        ComputeState m_CurrComputeState{};
        MeshletState m_CurrMeshletState{};
        bool m_CurrGraphicsStateValid = false;
        bool m_CurrComputeStateValid = false;
        bool m_CurrMeshletStateValid = false;

        std::vector<D3D12_RESOURCE_BARRIER> m_Barriers;

        ID3D12DescriptorHeap* m_CurrSRVHeap = nullptr;
        ID3D12DescriptorHeap* m_CurrSamplerHeap = nullptr;
    };

} // namespace DSM::D3D12 


#endif
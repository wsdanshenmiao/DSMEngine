#pragma once
#ifndef __D3D12_H__
#define __D3D12_H__

#include "Device.h"
#include <d3d12.h>

namespace DSM::D3D12{
    struct DynamicResourceLocation;

    struct IRootSignature : public IResource {};
    using RootSignatureHandle = RefPtr<IRootSignature>;

    struct ICommandList : public DSM::ICommandList
    {
        virtual DynamicResourceLocation AllocateUploadBuffer(size_t size) = 0;
        virtual DynamicResourceLocation AllocateGpuBuffer(size_t size) = 0;
        virtual bool CommitDescriptorHeaps() = 0;
        virtual D3D12_GPU_VIRTUAL_ADDRESS GetBufferGpuVA(IBuffer* buffer) = 0;

        virtual void UpdateGraphicsVolatileBuffers() = 0;
        virtual void UpdateComputeVolatileBuffers() = 0;
    };
    using CommandListHandle = RefPtr<ICommandList>;

    
    class IDescriptorHeap
    {
    protected:
        IDescriptorHeap() = default;
        virtual ~IDescriptorHeap() = default;

    public:
        virtual uint32_t AllocateDescriptors(uint32_t count) = 0;
        virtual uint32_t AllocateDescriptor() = 0;
        virtual void ReleaseDescriptors(uint32_t baseIndex, uint32_t count) = 0;
        virtual void ReleaseDescriptor(uint32_t index) = 0;

        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t index) = 0;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandleShaderVisible(uint32_t index) = 0;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t index) = 0;
        virtual uint32_t GetOffsetOfCpuHandle(size_t descriptorIndex) const = 0;
        virtual uint32_t GetOffsetOfGpuHandle(size_t descriptorIndex) const = 0;
        virtual uint32_t GetOffsetOfCpuHandleShaderVisible(size_t descriptorIndex) const = 0;

        [[nodiscard]] virtual ID3D12DescriptorHeap* GetHeap() const = 0;
        [[nodiscard]] virtual ID3D12DescriptorHeap* GetShaderVisibleHeap() const = 0;

        IDescriptorHeap(const IDescriptorHeap&) = delete;
        IDescriptorHeap(const IDescriptorHeap&&) = delete;
        IDescriptorHeap& operator=(const IDescriptorHeap&) = delete;
        IDescriptorHeap& operator=(const IDescriptorHeap&&) = delete;
    };

    enum class DescriptorHeapType
    {
        RenderTargetView,
        DepthStencilView,
        ShaderResourceView,
        Sampler
    };

    struct IDevice : public DSM::IDevice
    {
        virtual RootSignatureHandle BuildRootSignature(
            const StaticVector<BindingLayoutHandle, c_MaxBindingLayouts>& pipelineLayouts, 
            bool allowInputLayout, 
            bool isLocal, 
            const D3D12_ROOT_PARAMETER1* pCustomParameters = nullptr, 
            uint32_t numCustomParameters = 0) = 0;

        virtual GraphicsPipelineHandle CreateHandleForNativeGraphicsPipeline(
            IRootSignature* rootSignature, 
            ID3D12PipelineState* pipelineState, 
            const GraphicsPipelineDesc& desc, 
            const FramebufferInfo& framebufferInfo) = 0;

        virtual MeshletPipelineHandle CreateHandleForNativeMeshletPipeline(
            IRootSignature* rootSignature, 
            ID3D12PipelineState* pipelineState, 
            const MeshletPipelineDesc& desc, 
            const FramebufferInfo& framebufferInfo) = 0;

        [[nodiscard]] virtual IDescriptorHeap* GetDescriptorHeap(DescriptorHeapType heapType) = 0;
    };
    using DeviceHandle = RefPtr<IDevice>;

    struct DeviceDesc
    {
        IMessageCallback* errorCB = nullptr;

        uint32_t renderTargetViewHeapSize = 1024;
        uint32_t depthStencilViewHeapSize = 1024;
        uint32_t shaderResourceViewHeapSize = 16384;
        uint32_t samplerHeapSize = 1024;
        uint32_t maxTimerQueries = 256;

        // If enabled and the device has the capability,
        // create RootSignatures with D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED 
        // and D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
        bool enableHeapDirectlyIndexed = false;

        // 使用 IMessageCallback 来记录 Buffer 的生命周期
        bool logBufferLifetime = false;
    };

    DeviceHandle CreateDevice(const DeviceDesc& desc);

    DXGI_FORMAT ConvertFormat(DSM::Format format);

}

#endif
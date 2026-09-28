#pragma once
#ifndef __D3D12_BUFFER_H__
#define __D3D12_BUFFER_H__

#include "Runtime/Graphics/Buffer.h"
#include "Runtime/Graphics/D3D12/D3D12Common.h"

namespace DSM::D3D12{
    struct Context;
    class DeviceResources;

    class Buffer : public IBuffer
    {
    public:
        Buffer(const Context& context, std::shared_ptr<DeviceResources> resources)
            : m_Context(context), m_Resources(resources) { }
        ~Buffer() override { Destroy(); };

        bool Create(BufferDesc desc);
        void Create(BufferDesc desc, ID3D12Resource* resource);
        void Destroy();

        const BufferDesc& GetDesc() const override { return m_Desc; }
        GpuVirtualAddress GetGpuVirtualAddress() const override { return m_GpuVA; }
        // 虚拟缓冲绑定 placed resource 后回填 GPU 虚拟地址。
        void UpdateGpuVirtualAddress() { m_GpuVA = resource != nullptr ? resource->GetGPUVirtualAddress() : 0; }

        Object GetNativeObject(ObjectType type) override;

        // 创建资源视图
        void CreateCBV(size_t descriptor, BufferRange range) const;
        void CreateSRV(size_t descriptor, Format format, BufferRange range, ResourceType type) const;
        void CreateUAV(size_t descriptor, Format format, BufferRange range, ResourceType type) const;

        uint32_t GetClearUAV();

        // 创建空的资源视图
        static void CreateNullSRV(size_t descriptor, Format format, const Context& context);
        static void CreateNullUAV(size_t descriptor, Format format, const Context& context);
        

    public:
        RefPtr<ID3D12Resource> resource{};
        D3D12_RESOURCE_DESC resourceDesc{};

        HeapHandle heap{};
        
        HANDLE sharedHandle{};

        // 上一个使用该资源的命令列表的信号量和信号值，用于在资源被销毁时等待 GPU 完成对该资源的使用
        RefPtr<ID3D12Fence> lastUseFence{};
        uint64_t lastUseFenceValue{};

    private:
        BufferDesc m_Desc;
        const Context& m_Context;
        std::weak_ptr<DeviceResources> m_Resources;
        D3D12_GPU_VIRTUAL_ADDRESS m_GpuVA{};

        uint32_t m_ClearUAV = c_InvalidDescriptorIndex;
    };

} // namespace DSM

#endif
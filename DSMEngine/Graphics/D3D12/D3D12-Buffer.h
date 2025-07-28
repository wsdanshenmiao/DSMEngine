#pragma once
#ifndef __D3D12_BUFFER_H__
#define __D3D12_BUFFER_H__

#include "D3D12-Device.h"

namespace DSM::D3D12{
    class Buffer : public IBuffer
    {
    public:
        Buffer(const Context& context, DeviceResources& resources, BufferDesc desc)
            : m_Context(context), m_Resources(resources), m_Desc(std::move(desc)){}
        ~Buffer() override;

        const BufferDesc& GetDesc() const override { return m_Desc; }
        GpuVirtualAddress GetGpuVirtualAddress() const override { return m_GpuVA; }

        Object GetNativeObject(ObjectType type) override;

        void CreateCBV(size_t descriptor, BufferRange range) const;
        void CreateSRV(size_t descriptor, Format format, BufferRange range, ResourceType type) const;
        void CreateUAV(size_t descriptor, Format format, BufferRange range, ResourceType type) const;
        

    public:
        RefPtr<ID3D12Resource> resource{};
        HeapHandle heap{};
        HANDLE sharedHandle{};

        RefPtr<ID3D12Fence> lastUseFence{};
        uint64_t lastUseFenceValue{};

    private:
        const BufferDesc m_Desc;
        const Context& m_Context;
        DeviceResources& m_Resources;
        D3D12_GPU_VIRTUAL_ADDRESS m_GpuVA{};

        uint32_t m_ClearUAV = c_InvalidDescriptorIndex;
    };

} // namespace DSM

#endif
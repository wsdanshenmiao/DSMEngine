#pragma once
#ifndef __D3D12D_DESCRIPTORHEAP_H__
#define __D3D12D_DESCRIPTORHEAP_H__

#include "D3D12Common.h"
#include "Utils/LinearAllocator.h"

namespace DSM::D3D12{
    // 静态长度的描述符堆
    class DescriptorHeap : public IDescriptorHeap
    {
    public:
        explicit DescriptorHeap(const Context& context);

        void AllocateResource(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool shaderVisible);
        void CopyToShaderVisibleHeap(uint32_t index, uint32_t count);

        uint32_t AllocateDescriptors(uint32_t count) override;
        uint32_t AllocateDescriptor() override;
        void ReleaseDescriptors(uint32_t baseIndex, uint32_t count) override;
        void ReleaseDescriptor(uint32_t index) override;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t index) override;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandleShaderVisible(uint32_t index) override;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t index) override;
        [[nodiscard]] ID3D12DescriptorHeap* GetHeap() const override;
        [[nodiscard]] ID3D12DescriptorHeap* GetShaderVisibleHeap() const override;

    private:
        void Grow(uint32_t requireSize);

    private:
        const Context& m_Context;

        D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        
        // GPU 不可见的描述符堆和可见的描述符堆
        RefPtr<ID3D12DescriptorHeap> m_Heap;
        RefPtr<ID3D12DescriptorHeap> m_ShaderVisibleHeap;

        // 当前类型描述符的大小
        uint32_t m_Stride = 0;

        // 用于分配描述符的辅助变量
        D3D12_CPU_DESCRIPTOR_HANDLE m_StartCpuHandle = {0};
        D3D12_CPU_DESCRIPTOR_HANDLE m_StartCpuHandleShaderVisible = {0};
        D3D12_GPU_DESCRIPTOR_HANDLE m_StartGpuHandleShaderVisible ={0};
        
        LinearAllocator m_Allocator;

        std::mutex m_Mutex;
    };
}

#endif
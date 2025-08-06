#include "DescriptorHeap.h"
#include "Math/MathCommon.h"
#include "D3D12Common.h"

namespace DSM::D3D12{
    DescriptorHeap::DescriptorHeap(const Context &context)
        :m_Context(context){}

    void DescriptorHeap::AllocateResource(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors, bool shaderVisible)
    {
        std::lock_guard lock{m_Mutex};

        m_Heap = nullptr;
        m_ShaderVisibleHeap = nullptr;

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = heapType;
        heapDesc.NumDescriptors = numDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        auto hr = m_Context.m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_Heap.GetAddressOf()));

        if(FAILED(hr)){
            m_Context.Error("Descriptor heap create failed");
            return;
        }

        m_StartCpuHandle = m_Heap->GetCPUDescriptorHandleForHeapStart();
        m_HeapType = heapType;
        m_Stride = m_Context.m_Device->GetDescriptorHandleIncrementSize(heapType);
        m_Allocator.Clear();
        m_Allocator.Resize(numDescriptors);

        if(shaderVisible){
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            auto hr = m_Context.m_Device->CreateDescriptorHeap(
                &heapDesc, IID_PPV_ARGS(m_ShaderVisibleHeap.GetAddressOf()));
            if(FAILED(hr)){
                m_Context.Error("ShaderVisible descriptor heap create failed");
                return;
            }

            m_StartCpuHandleShaderVisible = m_ShaderVisibleHeap->GetCPUDescriptorHandleForHeapStart();
            m_StartGpuHandleShaderVisible = m_ShaderVisibleHeap->GetGPUDescriptorHandleForHeapStart();
        }
    }

    void DescriptorHeap::CopyToShaderVisibleHeap(uint32_t index, uint32_t count)
    {
        m_Context.m_Device->CopyDescriptorsSimple(count, 
            GetCpuHandleShaderVisible(index), GetCpuHandle(index), m_HeapType);
    }

    uint32_t DescriptorHeap::AllocateDescriptors(uint32_t count)
    {
        std::lock_guard lock{m_Mutex};

        auto index = m_Allocator.Allocate(count);
        if(index == LinearAllocator::InvalidAllocOffset){
            Grow(m_Allocator.Capacity() + count);
            index = m_Allocator.Allocate(count);
            assert(index != LinearAllocator::InvalidAllocOffset);
        }
        return index;
    }

    uint32_t DescriptorHeap::AllocateDescriptor()
    {
        return AllocateDescriptors(1);
    }

    void DescriptorHeap::ReleaseDescriptors(uint32_t baseIndex, uint32_t count)
    {
        std::lock_guard lock{m_Mutex};

        assert(m_Allocator.Deallocate(baseIndex, count));
    }

    void DescriptorHeap::ReleaseDescriptor(uint32_t index)
    {
        ReleaseDescriptors(index, 1);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuHandle(uint32_t index)
    {
        assert(index < m_Allocator.Capacity());
        return D3D12_CPU_DESCRIPTOR_HANDLE{m_StartCpuHandle.ptr + index * m_Stride};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuHandleShaderVisible(uint32_t index)
    {
        assert(index < m_Allocator.Capacity());
        return D3D12_CPU_DESCRIPTOR_HANDLE{m_StartCpuHandleShaderVisible.ptr + index * m_Stride};
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpuHandle(uint32_t index)
    {
        assert(index < m_Allocator.Capacity());
        return D3D12_GPU_DESCRIPTOR_HANDLE{m_StartGpuHandleShaderVisible.ptr + index * m_Stride};
    }

    uint32_t DescriptorHeap::GetOffsetOfCpuHandle(size_t descriptor) const
    {
        auto end = m_StartCpuHandle.ptr + m_Allocator.Capacity() * m_Stride;
        assert(m_StartCpuHandle.ptr <= descriptor && descriptor < end);
        return static_cast<uint32_t>(descriptor - m_StartCpuHandle.ptr) / m_Stride;
    }

    uint32_t DescriptorHeap::GetOffsetOfGpuHandle(size_t descriptor) const
    {
        auto end = m_StartGpuHandleShaderVisible.ptr + m_Allocator.Capacity() * m_Stride;
        assert(m_StartGpuHandleShaderVisible.ptr <= descriptor && descriptor < end);
        return static_cast<uint32_t>(descriptor - m_StartGpuHandleShaderVisible.ptr) / m_Stride;
    }

    uint32_t DescriptorHeap::GetOffsetOfCpuHandleShaderVisible(size_t descriptor) const
    {
        auto end = m_StartCpuHandleShaderVisible.ptr + m_Allocator.Capacity() * m_Stride;
        assert(m_StartCpuHandleShaderVisible.ptr <= descriptor && descriptor < end);
        return static_cast<uint32_t>(descriptor - m_StartCpuHandleShaderVisible.ptr) / m_Stride;
    }

    ID3D12DescriptorHeap *DescriptorHeap::GetHeap() const
    {
        return m_Heap.Get();
    }

    ID3D12DescriptorHeap *DescriptorHeap::GetShaderVisibleHeap() const
    {
        return m_ShaderVisibleHeap.Get();
    }

    void DescriptorHeap::Grow(uint32_t requireSize)
    {
        uint32_t preSize = m_Allocator.Capacity();
        uint32_t newSize = NextPowerOf2(requireSize);

        bool shaderVisible = m_ShaderVisibleHeap != nullptr;
        if(shaderVisible){
            const uint32_t maxSize = m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ?
                D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1 :
                D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
            
            newSize = std::max(newSize, maxSize);
            if(newSize < requireSize){
                m_Context.Error("DescriptorHeap out of memory");
            }
        }

        RefPtr<ID3D12DescriptorHeap> oldHeap = m_Heap;
        AllocateResource(m_HeapType, newSize, shaderVisible);

        // 拷贝源堆的描述符
        auto& device = m_Context.m_Device;
        device->CopyDescriptorsSimple(preSize, m_StartCpuHandle,
            oldHeap->GetCPUDescriptorHandleForHeapStart(), m_HeapType);
        if(shaderVisible){
            device->CopyDescriptorsSimple(preSize, m_StartCpuHandleShaderVisible,
                oldHeap->GetCPUDescriptorHandleForHeapStart(), m_HeapType);
        }
    }
}
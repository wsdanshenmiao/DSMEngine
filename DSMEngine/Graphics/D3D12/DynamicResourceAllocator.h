#pragma once
#ifndef __LINEARBUFFERALLOCATOR_H__
#define __LINEARBUFFERALLOCATOR_H__

#include <d3d12.h>
#include <queue>
#include <optional>
#include "Graphics/GraphicsCommon.h"
#include "Utils/LinearAllocator.h"

namespace DSM::D3D12 {
    struct Context;
    class CommandQueue;

    // 用于定位子资源在缓冲区中的位置
    struct DynamicResourceLocation
    {
        ID3D12Resource* resource = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress{};
        void* mappedAddress = nullptr;
        uint64_t offset{};
        uint64_t size{};
    };
    
    class DynamicResourcePage
    {
        friend class DynamicResourceAllocator;
    public:
        DynamicResourcePage(ID3D12Resource* agentResource, bool mappedAble) 
            :m_Resource(agentResource), m_LiearAllocator(agentResource->GetDesc().Width)
        {
            if (mappedAble) {
                m_Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedAddress));
            }
        }
        ~DynamicResourcePage()
        {
            if(m_MappedAddress != nullptr){
                m_Resource->Unmap(0, nullptr);
                m_MappedAddress = nullptr;
            }
        }

        std::optional<DynamicResourceLocation> Allocate(uint64_t size, uint32_t alignment)
        {
            std::optional<DynamicResourceLocation> ret{};
            auto offset = m_LiearAllocator.Allocate(size, alignment);
            if (offset != LinearAllocator::InvalidAllocOffset) {
                DynamicResourceLocation outResource{};
                outResource.resource = m_Resource.Get();
                outResource.offset = offset;
                outResource.size = size;
                outResource.gpuAddress = m_Resource->GetGPUVirtualAddress() + offset;
                if (m_MappedAddress != nullptr) {
                    outResource.mappedAddress = m_MappedAddress + offset;
                }
                ret = std::make_optional<DynamicResourceLocation>(std::move(outResource));
            }
            return ret;
        }
        
        void Cleanup() noexcept
        {
            m_LiearAllocator.Clear();
        }

    private:
        RefPtr<ID3D12Resource> m_Resource{};
        LinearAllocator m_LiearAllocator;
        uint8_t* m_MappedAddress{};
    };
    
    class DynamicResourceAllocator
    {
    public:
        enum class AllocateMode
        {
            CpuExclusive, GpuExclusive
        };
        
        DynamicResourceAllocator(const Context& context, CommandQueue* queue, AllocateMode mode, uint64_t pageSize);
        ~DynamicResourceAllocator();
        DynamicResourceAllocator(const DynamicResourceAllocator&) = delete;

        DynamicResourceLocation Allocate(uint64_t bufferSize, uint32_t alignment = 0);
        // 清理所有的缓冲区
        void Cleanup(std::uint64_t fenceValue);

    private:
        DynamicResourcePage* RequestPage();
        ID3D12Resource* CreateNewResource(uint64_t bufferSize = 0);

    private:
        const Context& m_Context;
        CommandQueue* const m_Queue;
    
        const AllocateMode m_AllocateMode = AllocateMode::CpuExclusive;
        const uint64_t m_PageSize{};
        
                
        std::vector<std::unique_ptr<DynamicResourcePage>> m_PagePool;

        std::vector<DynamicResourcePage*> m_FullPages;
        std::vector<ID3D12Resource*> m_LargePages;

        DynamicResourcePage* m_CurrPage{};
        // 等待使用完毕的资源
        std::queue<std::pair<std::uint64_t, DynamicResourcePage*>> m_RetiredPages{};
        // 可重复使用的资源
        std::queue<DynamicResourcePage*> m_AvailablePages{};
        // 需要删除的资源
        std::queue<std::pair<std::uint64_t, ID3D12Resource*>> m_DeletionPages{};

        std::mutex m_Mutex{};
        
    };

}

#endif


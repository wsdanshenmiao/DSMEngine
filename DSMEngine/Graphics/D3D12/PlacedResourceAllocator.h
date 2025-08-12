#pragma once
#ifndef __GPURESOURCEALLOCATOR_H__
#define __GPURESOURCEALLOCATOR_H__

#include <set>
#include <d3d12.h>
#include <map>
#include <queue>
#include "Utils/LinearAllocator.h"
#include "Graphics/GraphicsCommon.h"

namespace DSM::D3D12 {
    struct Context;

    // 用于管理一个堆的内存分配,不负责管理资源的释放
    class PlacedResourcePage
    {
        friend class PlacedResourceAllocator;
    public:
        PlacedResourcePage(const Context& context, ID3D12Heap* agentHeap)
            :m_Context(context), m_Heap(agentHeap), m_Allocator(agentHeap->GetDesc().SizeInBytes){}

        PlacedResourcePage(const PlacedResourcePage&) = delete;
        PlacedResourcePage(PlacedResourcePage&&) = delete;

        ID3D12Resource* Allocate(
            const D3D12_RESOURCE_DESC& resourceDesc,
            D3D12_RESOURCE_STATES resourceState,
            const D3D12_CLEAR_VALUE* clearValue,
            uint64_t resourceSize,
            uint32_t alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
        bool ReleaseResource(ID3D12Resource* resource);

        void Cleanup() noexcept
        {
            m_SubResources.clear();
            m_Allocator.Clear();
        }
        inline size_t GetSubresourcesCount() const noexcept{ return m_SubResources.size(); }

        inline bool Full() const noexcept { return m_Allocator.Full(); }
        inline bool Empty() const noexcept { return m_SubResources.empty(); }
        
    private:
        const Context& m_Context;
        RefPtr<ID3D12Heap> m_Heap;
        std::set<ID3D12Resource*> m_SubResources{};
        LinearAllocator m_Allocator;
    };

    // 用于管理所有的资源分配
    class PlacedResourceAllocator
    {
    public:
        struct AllocatorDesc
        {    
            D3D12_HEAP_TYPE m_HeapType{};
            D3D12_HEAP_FLAGS m_HeapFlags = D3D12_HEAP_FLAG_NONE;
            uint64_t m_HeapSize{};
            std::strong_ordering operator<=>(const AllocatorDesc& rhs) const = default;
        };

    public:
        PlacedResourceAllocator(const Context& context, AllocatorDesc desc);
        ~PlacedResourceAllocator();
        PlacedResourceAllocator(const PlacedResourceAllocator&) = delete;

        ID3D12Resource* CreateResource(
            const D3D12_RESOURCE_DESC& resourceDesc,
            D3D12_RESOURCE_STATES resourceState,
            const D3D12_CLEAR_VALUE* clearValue = nullptr);
        void ReleaseResource(ID3D12Resource* resource);
        
        ID3D12Heap* CreateNewHeap(uint64_t heapSize = 0);
        

    private:
        PlacedResourcePage* RequestPage();

    private:
        const Context& m_Context;
        const AllocatorDesc m_Desc{};
        
        std::vector<std::unique_ptr<PlacedResourcePage>> m_PagePool{};
        
        PlacedResourcePage* m_CurrPage{};
        std::set<PlacedResourcePage*> m_FullPages{};
        // 建立各个资源与分配者的映射关系，便于快速索引
        std::map<ID3D12Resource*, PlacedResourcePage*> m_ResourceMappings{};
        // 可重复使用的资源
        std::queue<PlacedResourcePage*> m_AvailablePages{};
        
        std::mutex m_Mutex{};
    };

}


#endif


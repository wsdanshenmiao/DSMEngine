#include "PlacedResourceAllocator.h"
#include "Graphics/D3D12/D3D12Common.h"

namespace DSM::D3D12 {
    //
    // PlacedResourcePage Implementation
    //
    ID3D12Resource* PlacedResourcePage::Allocate(
        const D3D12_RESOURCE_DESC& resourceDesc,
        D3D12_RESOURCE_STATES resourceState,
        const D3D12_CLEAR_VALUE* clearValue,
        uint64_t resourceSize,
        uint32_t alignment)
    {
        auto offset = m_Allocator.Allocate(resourceSize, alignment);
        ID3D12Resource* resource = nullptr;
        if (offset != LinearAllocator::InvalidAllocOffset) {
            auto hr = m_Context.device->CreatePlacedResource(
                m_Heap.Get(), offset, &resourceDesc,
                resourceState, clearValue, IID_PPV_ARGS(&resource));
            if(FAILED(hr)){
                m_Context.Error("Create placed resource failed.");
                return nullptr;
            }
                
            m_SubResources.insert(resource);
        }
        return resource;
    }

    bool PlacedResourcePage::ReleaseResource(ID3D12Resource* resource)
    {
        if (m_SubResources.contains(resource)) {
            m_SubResources.erase(resource);
            return true;
        }
        else{
            return false;
        }
    }

    //
    // PlacedResourceAllocator Implementation
    //
    PlacedResourceAllocator::PlacedResourceAllocator(const Context& context, AllocatorDesc desc)
        :m_Context(context), m_Desc(std::move(desc)){
        std::lock_guard lock{m_Mutex};
        
        auto newHeap = CreateNewHeap();
        auto newPage = std::make_unique<PlacedResourcePage>(m_Context, newHeap);
        m_CurrPage = newPage.get();
        m_PagePool.emplace_back(std::move(newPage));
    }

    PlacedResourceAllocator::~PlacedResourceAllocator()
    {
        std::lock_guard lock{m_Mutex};
        
        while (!m_AvailablePages.empty()) {
            m_AvailablePages.pop();
        }
        m_FullPages.clear();
        m_ResourceMappings.clear();
        m_CurrPage = nullptr;
        m_PagePool.clear();
    }

    ID3D12Resource* PlacedResourceAllocator::CreateResource(
        const D3D12_RESOURCE_DESC& resourceDesc,
        D3D12_RESOURCE_STATES resourceState,
            const D3D12_CLEAR_VALUE* clearValue)
    {
        auto allocInfo = m_Context.device->GetResourceAllocationInfo(0, 1, &resourceDesc);
        
        std::lock_guard lock{m_Mutex};
        
        ID3D12Resource* resource = nullptr;
        if (allocInfo.SizeInBytes > m_Desc.m_HeapSize) {    // 过大的资源不创建堆
            D3D12_HEAP_PROPERTIES prop = {};
            prop.Type = m_Desc.m_HeapType;
            prop.CreationNodeMask = 1;
            prop.VisibleNodeMask = 1;
            m_Context.device->CreateCommittedResource(
                &prop,
                m_Desc.m_HeapFlags,
                &resourceDesc,
                resourceState,
                clearValue,
                IID_PPV_ARGS(&resource));
        }
        else {
            resource = m_CurrPage->Allocate(resourceDesc, resourceState, clearValue, 
                allocInfo.SizeInBytes, allocInfo.Alignment);
            if (resource == nullptr) {
                m_FullPages.insert(m_CurrPage);
                m_CurrPage = RequestPage();
                resource = m_CurrPage->Allocate(resourceDesc, resourceState, clearValue, 
                    allocInfo.SizeInBytes, allocInfo.Alignment);
            }

            // 只对在堆中分配的资源进行映射
            m_ResourceMappings.insert(std::make_pair(resource, m_CurrPage));
        }

        return resource;
    }

    void PlacedResourceAllocator::ReleaseResource(ID3D12Resource* resource)
    {
        assert(resource != nullptr);
        
        std::lock_guard lock{m_Mutex};

        if (m_CurrPage != nullptr && !m_CurrPage->ReleaseResource(resource) && 
            m_ResourceMappings.contains(resource)) {
            auto it = m_FullPages.find(m_ResourceMappings[resource]);
            assert((*it)->ReleaseResource(resource));
            if ((*it)->Empty()) {
                (*it)->Cleanup();
                m_AvailablePages.push(*it);
                m_FullPages.erase(it);
            }
            m_ResourceMappings.erase(resource);
        }
    }

    PlacedResourcePage* PlacedResourceAllocator::RequestPage()
    {
        PlacedResourcePage* page = nullptr;
        // 清除已经完成的资源
        if (m_AvailablePages.empty()) {
            auto newHeap = CreateNewHeap();
            page = new PlacedResourcePage{m_Context, newHeap};
            m_PagePool.emplace_back(page);
        }
        else {
            page = m_AvailablePages.front();
            m_AvailablePages.pop();
        }
        
        return page;
    }

    ID3D12Heap* PlacedResourceAllocator::CreateNewHeap(uint64_t heapSize)
    {
        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = m_Desc.m_HeapType;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;
        
        D3D12_HEAP_DESC heapDesc{};
        heapDesc.Flags = m_Desc.m_HeapFlags;
        heapDesc.Properties = heapProperties;
        heapDesc.SizeInBytes = heapSize == 0 ? m_Desc.m_HeapSize : heapSize;

        ID3D12Heap* heap = nullptr;
        m_Context.device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap));
        auto heapName = L"PlacedResourceAllocator Heap" + std::to_wstring(m_PagePool.size());
        heap->SetName(heapName.c_str());
        
        return heap;
    }




}

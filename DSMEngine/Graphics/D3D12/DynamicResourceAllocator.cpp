#include "DynamicResourceAllocator.h"
#include "D3D12-Device.h"

namespace DSM::D3D12 {
    DynamicResourceAllocator::DynamicResourceAllocator(
        const Context& context, CommandQueue* queue, 
        AllocateMode mode, std::uint64_t pageSize)
        :m_Context(context), m_Queue(queue), m_AllocateMode(mode), m_PageSize(pageSize) {
        std::lock_guard lock{m_Mutex};

        auto buffer = CreateNewResource();
        assert(buffer != nullptr);
        bool mappedAble = m_AllocateMode == AllocateMode::CpuExclusive;
        auto newPage = std::make_unique<DynamicResourcePage>(buffer, mappedAble);
        m_CurrPage = newPage.get();
        m_PagePool.emplace_back(std::move(newPage));
    }

    DynamicResourceAllocator::~DynamicResourceAllocator()
    {
        std::lock_guard lock{m_Mutex};
        
        m_CurrPage = nullptr;
        m_FullPages.clear();
        m_LargePages.clear();
        while (!m_RetiredPages.empty()) {
            m_RetiredPages.pop();
        }
        while (!m_AvailablePages.empty()) {
            m_AvailablePages.pop();
        }
        while (!m_DeletionPages.empty()) {
            m_DeletionPages.pop();
        }
        m_PagePool.clear();
    }
    
    DynamicResourceLocation DynamicResourceAllocator::Allocate(uint64_t bufferSize, uint32_t alignment)
    {
        std::optional<DynamicResourceLocation> ret{};

        std::lock_guard lock(m_Mutex);

        // 过大的资源额外管理
        if (auto alignSize = Utility::Align(bufferSize, uint64_t(alignment)); alignSize > m_PageSize) {
            DynamicResourceLocation location{};
            location.resource = CreateNewResource(alignSize);
            location.size = alignSize;
            location.gpuAddress = location.resource->GetGPUVirtualAddress();
            if (m_AllocateMode == AllocateMode::CpuExclusive) {
                location.resource->Map(0, nullptr, &location.mappedAddress);
            }
            m_LargePages.push_back(location.resource);
            ret = std::make_optional(std::move(location));
        }
        else if (ret = m_CurrPage->Allocate(bufferSize, alignment); !ret.has_value()) {    // 创建新的Page
            // 记录已经满的Page
            if (m_CurrPage != nullptr) {
                m_FullPages.push_back(m_CurrPage);
            }
            m_CurrPage = RequestPage();
            ret = m_CurrPage->Allocate(bufferSize, alignment);
        }
        
        assert(ret.has_value());
        return *ret;
    }

    void DynamicResourceAllocator::Cleanup(std::uint64_t fenceValue)
    {
        std::lock_guard lock{m_Mutex};
        
        for (auto& fullPage : m_FullPages) {
            fullPage->Cleanup();
            m_RetiredPages.push(std::make_pair(fenceValue, fullPage));
        }
        m_FullPages.clear();

        while (!m_DeletionPages.empty() && m_Queue->IsFenceComplete(fenceValue)) {
            m_DeletionPages.front().second->Release();
            m_DeletionPages.pop();
        }

        for (auto& page : m_LargePages) {
            if(m_AllocateMode == AllocateMode::CpuExclusive){
                page->Unmap(0, nullptr);
            }
            m_DeletionPages.push(std::make_pair(fenceValue, page));
        }
        m_LargePages.clear();
    }

    DynamicResourcePage* DynamicResourceAllocator::RequestPage()
    {
        // 清除已经完成的资源
        while (!m_RetiredPages.empty() &&
            m_Queue->IsFenceComplete(m_RetiredPages.front().first)) {
            m_AvailablePages.push(m_RetiredPages.front().second);
            m_RetiredPages.pop();
        }

        DynamicResourcePage* ret = nullptr;
        if (m_AvailablePages.empty()) {
            bool mappedAble = m_AllocateMode == AllocateMode::CpuExclusive;
            auto newPage = std::make_unique<DynamicResourcePage>(CreateNewResource(), mappedAble);
            ret = newPage.get();
            m_PagePool.emplace_back(std::move(newPage));
        }
        else {
            ret = m_AvailablePages.front();
            m_AvailablePages.pop();
        }
        
        return ret;
    }

    ID3D12Resource* DynamicResourceAllocator::CreateNewResource(std::uint64_t bufferSize)
    {
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Width = bufferSize == 0 ? m_PageSize : bufferSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.Alignment = 0;
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc = {1, 0};
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES heapProp{};
        heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        
        if ( m_AllocateMode == AllocateMode::GpuExclusive ) {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
            resourceState = D3D12_RESOURCE_STATE_COMMON;
        }
        
        ID3D12Resource* ret = nullptr;
        auto hr = m_Context.device->CreateCommittedResource(
            &heapProp, 
            D3D12_HEAP_FLAG_NONE, 
            &resourceDesc, 
            resourceState, 
            nullptr, 
            IID_PPV_ARGS(&ret));
        
        if(FAILED(hr)){
            m_Context.Error("Create committed resource failed on DynamicResourceAllocator.");
            ret = nullptr;
        }

        return ret;
    }

}

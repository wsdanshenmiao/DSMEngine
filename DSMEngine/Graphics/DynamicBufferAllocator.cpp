#include "DynamicBufferAllocator.h"
#include "RenderContext.h"

namespace DSM {
    void DynamicBufferAllocator::Create(std::uint64_t pageSize)
    {
        std::lock_guard lock{m_Mutex};
        
        m_PageSize = pageSize;

        auto buffer = CreateNewBuffer();
        auto newPage = std::make_unique<DynamicBufferPage>(buffer);
        m_CurrPage = newPage.get();
        m_PagePool.emplace_back(std::move(newPage));
    }

    void DynamicBufferAllocator::Shutdown()
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
        for (auto& page : m_PagePool) {
            page->m_Resource->GetResource()->Unmap(0, nullptr);
        }
        m_PagePool.clear();
    }

    GpuResourceLocatioin DynamicBufferAllocator::Allocate(std::uint64_t bufferSize, std::uint32_t alignment)
    {
        GpuResourceLocatioin ret{};

        std::lock_guard lock(m_Mutex);

        // 过大的资源额外管理
        if (auto alignSize = Utility::AlignUp(bufferSize, alignment) > m_PageSize) {
            ret.m_Resource = CreateNewBuffer(alignSize);
            ret.m_Size = alignSize;
            ret.m_GpuAddress = ret.m_Resource->GetGpuVirtualAddress();
            ASSERT_SUCCEEDED(ret.m_Resource->GetResource()->Map(0, nullptr, &ret.m_MappedAddress));
            m_LargePages.push_back(ret.m_Resource);
        }
        else if (m_CurrPage == nullptr || !m_CurrPage->Allocate(bufferSize, alignment, ret)) {    // 创建新的Page
            // 记录已经满的Page
            if (m_CurrPage != nullptr) {
                m_FullPages.push_back(m_CurrPage);
            }
            m_CurrPage = RequestPage();
            m_CurrPage->Allocate(bufferSize, alignment, ret);
        }
        
        return ret;
    }

    void DynamicBufferAllocator::Cleanup(std::uint64_t fenceValue)
    {
        std::lock_guard lock{m_Mutex};
        
        if (m_CurrPage != nullptr) {
            m_FullPages.push_back(m_CurrPage);
            m_CurrPage = nullptr;
        }

        for (auto& fullPage : m_FullPages) {
            fullPage->Reset();
            m_RetiredPages.push(std::make_pair(fenceValue, fullPage));
        }
        m_FullPages.clear();

        while (!m_DeletionPages.empty() && g_RenderContext.IsFenceComplete(fenceValue)) {
            delete m_DeletionPages.front().second;
            m_DeletionPages.pop();
        }

        for (auto& page : m_LargePages) {
            page->GetResource()->Unmap(0, nullptr);
            m_DeletionPages.push(std::make_pair(fenceValue, page));
        }
    }

    DynamicBufferPage* DynamicBufferAllocator::RequestPage()
    {
        // 清除已经完成的资源
        while (!m_RetiredPages.empty() &&
            g_RenderContext.IsFenceComplete(m_RetiredPages.front().first)) {
            m_AvailablePages.push(m_RetiredPages.front().second);
            m_RetiredPages.pop();
        }

        DynamicBufferPage* ret = nullptr;
        if (m_AvailablePages.empty()) {
            auto newPage = std::make_unique<DynamicBufferPage>(CreateNewBuffer());
            ret = newPage.get();
            m_PagePool.emplace_back(std::move(newPage));
        }
        else {
            ret = m_AvailablePages.front();
            m_AvailablePages.pop();
        }
        
        return ret;
    }

    GpuResource* DynamicBufferAllocator::CreateNewBuffer(std::uint64_t bufferSize)
    {
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Alignment = 0;
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.Height = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc = {1, 0};
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.Width = bufferSize == 0 ? m_PageSize : bufferSize;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;
        
        GpuResourceDesc bufferDesc{};
        bufferDesc.m_Desc = resourceDesc;
        bufferDesc.m_State = resourceState;
        bufferDesc.m_HeapType = D3D12_HEAP_TYPE_UPLOAD;
        auto ret = new GpuResource(bufferDesc);
        
        return ret;
    }




}

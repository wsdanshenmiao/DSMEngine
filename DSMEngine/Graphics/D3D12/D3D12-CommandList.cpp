#include "D3D12-CommandList.h"


namespace DSM::D3D12{

    Object CommandList::GetNativeObject(ObjectType type)
    {
        switch (type) {
        case ObjectTypes::D3D12_GraphicsCommandList:
            if(m_CurrCmdList != nullptr) return Object{m_CurrCmdList->cmdList.Get()};
            break;
        case ObjectTypes::D3D12_CommandAllocator:
            if(m_CurrCmdList != nullptr) return Object{m_CurrCmdList->allocator.Get()};
            break;
        default:
            return Object{nullptr};
        }
        return Object{nullptr};
    }

    void CommandList::Open()
    {
        m_CurrCmdList = RequireCommandList(m_Context, m_Desc.queueType);

        m_Instance = std::make_shared<CommandListInstance>();
        m_Instance->queueType = m_Desc.queueType;
        m_Instance->allocator = m_CurrCmdList->allocator;
        m_Instance->cmdList = m_CurrCmdList->cmdList;
    }

    void CommandList::Close()
    {
        CommitBarriers();
        m_CurrCmdList->cmdList->Close();
        ClearStateCache();
    }

    void CommandList::ClearState()
    {
        m_CurrCmdList->cmdList->ClearState(nullptr);
        ClearStateCache();
        CommitDescriptorHeaps();
    }

    void CommandList::Cleanup()
    {
        sm_CommandListPool.clear();
    }
    
    void CommandList::ClearStateCache()
    {
        m_CurrGraphicsStateValid = false;
        m_CurrComputeStateValid = false;
        m_CurrMeshletStateValid = false;
        m_CurrSRVHeap = nullptr;
        m_CurrSamplerHeap = nullptr;
    }
}
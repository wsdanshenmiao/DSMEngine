#include "D3D12-Buffer.h"


namespace DSM::D3D12{
    Buffer::~Buffer()
    {
        if(m_Context.m_LogBufferLifetime){
            m_Context.Info(std::format("Release buffer: {} {:#x}", 
                m_Desc.debugName, resource->GetGPUVirtualAddress()));
        }
        if(m_ClearUAV != c_InvalidDescriptorIndex){
            m_Resources.shaderResourceViewHeap.ReleaseDescriptor(m_ClearUAV);
            m_ClearUAV = c_InvalidDescriptorIndex;
        }
    }
    
    Object Buffer::GetNativeObject(ObjectType type)
    {
        switch (type)
        {
        case ObjectTypes::D3D12_Resource:
            return Object{resource};
        case ObjectTypes::SharedHandle:
            return Object{sharedHandle};
        default:
            return Object{nullptr};
        }
    }
}

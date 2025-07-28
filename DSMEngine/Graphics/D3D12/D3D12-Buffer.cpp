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
}


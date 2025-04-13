#pragma once
#ifndef __GPUBUFFER_H__
#define __GPUBUFFER_H__

#include "GpuResource.h"
#include "GraphicsCommon.h"

namespace DSM {
    // 用于创建 Buffer 的描述
    struct GpuBufferDesc
    {
        std::uint64_t m_Size = 0;
        DSMResourceUsage m_Usage = DSMResourceUsage::Default;
        DSMBindFlag m_BindFlag = DSMBindFlag::None;
        DSMBufferFlag m_BufferFlag = DSMBufferFlag::None;
        std::uint32_t m_Stride = 0;
        DXGI_FORMAT m_Format = DXGI_FORMAT_UNKNOWN;
    };

    
    // GPU 缓冲区
    class GpuBuffer : public GpuResource
    {
    public:
        GpuBuffer(const GpuBufferDesc& bufferDesc, void* initData = nullptr);

        void SetName(const std::wstring& name)
        {
            m_Resource->SetName(name.c_str());
        }

        const GpuBufferDesc& GetDesc() const noexcept { return m_BufferDesc; }
        std::uint64_t GetSize() const noexcept {return m_BufferDesc.m_Size; }
        std::uint32_t GetStride() const noexcept { return m_BufferDesc.m_Stride; }
        DXGI_FORMAT GetFormat() const noexcept { return m_BufferDesc.m_Format; }

        bool Mappable() const noexcept
        {
            return m_BufferDesc.m_Usage == DSMResourceUsage::Upload ||
                m_BufferDesc.m_Usage == DSMResourceUsage::Readback ;
        }
        void Update(void* data, std::uint64_t size, std::uint64_t offset = 0);

    protected:
        GpuBufferDesc m_BufferDesc;
    };

}

#endif
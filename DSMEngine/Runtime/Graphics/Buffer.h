#pragma once
#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "GraphicsCommon.h"

namespace DSM {
    struct BufferDesc
    {
        uint64_t byteSize = 0;
        uint32_t structStride = 0; // 若为 StructuredBuffer 改参数不为0
        std::string debugName;
        Format format = Format::UNKNOWN;
        bool canHaveUAVs = false;
        bool canHaveRawViews = false;
        bool isVertexBuffer = false;
        bool isIndexBuffer = false;
        bool isConstantBuffer = false;
        bool isDrawIndirectArgs = false;
        bool isAccelStructBuildInput = false;
        bool isAccelStructStorage = false;
        bool isShaderBindingTable = false;

        // 当前的命令列表销毁时，上传堆种创建的资源会被销毁
        bool isVolatile = false;

        bool isVirtual = false;

        ResourceStates initialState = ResourceStates::Common;

        bool keepInitialState = false;

        CpuAccessMode cpuAccess = CpuAccessMode::None;

        SharedResourceFlags sharedResourceFlags = SharedResourceFlags::None;

        constexpr BufferDesc& SetByteSize(uint64_t value) { byteSize = value; return *this; }
        constexpr BufferDesc& SetStructStride(uint32_t value) { structStride = value; return *this; }
        constexpr BufferDesc& SetFormat(Format value) { format = value; return *this; }
        constexpr BufferDesc& SetCanHaveUAVs(bool value) { canHaveUAVs = value; return *this; }
        constexpr BufferDesc& SetCanHaveRawViews(bool value) { canHaveRawViews = value; return *this; }
        constexpr BufferDesc& SetIsVertexBuffer(bool value) { isVertexBuffer = value; return *this; }
        constexpr BufferDesc& SetIsIndexBuffer(bool value) { isIndexBuffer = value; return *this; }
        constexpr BufferDesc& SetIsConstantBuffer(bool value) { isConstantBuffer = value; return *this; }
        constexpr BufferDesc& SetIsDrawIndirectArgs(bool value) { isDrawIndirectArgs = value; return *this; }
        constexpr BufferDesc& SetIsAccelStructBuildInput(bool value) { isAccelStructBuildInput = value; return *this; }
        constexpr BufferDesc& SetIsAccelStructStorage(bool value) { isAccelStructStorage = value; return *this; }
        constexpr BufferDesc& SetIsShaderBindingTable(bool value) { isShaderBindingTable = value; return *this; }
        constexpr BufferDesc& SetIsVolatile(bool value) { isVolatile = value; return *this; }
        constexpr BufferDesc& SetIsVirtual(bool value) { isVirtual = value; return *this; }
        constexpr BufferDesc& SetInitialState(ResourceStates value) { initialState = value; return *this; }
        constexpr BufferDesc& SetKeepInitialState(bool value) { keepInitialState = value; return *this; }
        constexpr BufferDesc& SetCpuAccess(CpuAccessMode value) { cpuAccess = value; return *this; }

       
        BufferDesc& SetDebugName(const std::string& value) { debugName = value; return *this; }
   };

   struct BufferRange
   {
        uint64_t byteOffset = 0;
        uint64_t byteSize = 0;

        BufferRange() = default;
        BufferRange(uint32_t offset, uint32_t size) : byteOffset(offset), byteSize(size) {}

        [[nodiscard]] BufferRange Resolve(const BufferDesc& desc) const
        {
            auto ret = *this;
            ret.byteOffset = std::min(byteOffset, desc.byteSize);
            if(byteSize == 0){
                ret.byteSize = desc.byteSize - byteOffset;
            }
            else{
                ret.byteSize = std::min(byteSize, desc.byteSize - byteOffset);
            }
            return ret;
        }

        [[nodiscard]] constexpr bool isEntireBuffer(const BufferDesc& desc) const 
        { 
            return (byteOffset == 0) && (byteSize == ~0ull || byteSize == desc.byteSize); 
        }
        
        constexpr bool operator== (const BufferRange& other) const = default;

        constexpr BufferRange& SetByteOffset(uint64_t value) { byteOffset = value; return *this; }
        constexpr BufferRange& SetByteSize(uint64_t value) { byteSize = value; return *this; }
   };

    static const BufferRange EntireBuffer = BufferRange(0, ~0ull);

    struct IBuffer : public IResource
    {
        [[nodiscard]] virtual const BufferDesc& GetDesc() const = 0;
        [[nodiscard]] virtual GpuVirtualAddress GetGpuVirtualAddress() const = 0;
    };
    using BufferHandle = RefPtr<IBuffer>;

} // namespace DSM 


template <>
struct std::hash<DSM::BufferRange>
{
    std::size_t operator()(const DSM::BufferRange& s) const noexcept
    {
        std::size_t hash = 0;
        hash = DSM::Utility::HashCombine(hash, s.byteOffset);
        hash = DSM::Utility::HashCombine(hash, s.byteSize);
        return hash; 
    }
};


#endif
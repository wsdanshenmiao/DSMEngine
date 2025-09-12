#pragma once
#ifndef __HEAP_H__
#define __HEAP_H__

#include "GraphicsCommon.h"

namespace DSM {
    enum class HeapType : uint8_t
    {
        Default,
        Upload,
        Readback
    };

    struct HeapDesc
    {
        uint64_t capacity = 0;
        HeapType type;
        std::string debugName;

        constexpr HeapDesc& SetCapacity(uint64_t value) { capacity = value; return *this; }
        constexpr HeapDesc& SetType(HeapType value) { type = value; return *this; }
        HeapDesc& SetDebugName(const std::string& value) { debugName = value; return *this; }

        bool operator==(const HeapDesc& other) const = default;
    };

    struct IHeap : public IResource
    {
        virtual const HeapDesc& GetDesc() const = 0;
    };
    using HeapHandle = RefPtr<IHeap>;
    
} // namespace DSM 


#endif
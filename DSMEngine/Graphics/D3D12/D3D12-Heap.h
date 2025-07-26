#pragma once
#ifndef __D3D12_HEAP_H__
#define __D3D12_HEAP_H__

#include "D3D12Common.h"

namespace DSM::D3D12 {
    class Heap : public IHeap
    {
    public:
        ID3D12Heap* GetHeap() const
        {
            return m_Heap.Get();
        }

        const HeapDesc& GetDesc() const override
        {
            return m_Desc;
        }

    private:
        HeapDesc m_Desc{};
        RefPtr<ID3D12Heap> m_Heap{};
    };
    
} // namespace DSM 

#endif
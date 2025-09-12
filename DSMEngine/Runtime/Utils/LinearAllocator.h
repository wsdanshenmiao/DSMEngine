#pragma once
#ifndef __LINEARALLOCATOR_H__
#define __LINEARALLOCATOR_H__


#include <list>
#include <cassert>
#include "Runtime/Math/MathCommon.h"

namespace DSM {
    
    class LinearAllocator
    {
    public:
        struct AllocationRange
        {
            uint64_t m_Start;
            uint64_t m_Size;

            std::strong_ordering operator<=>(const AllocationRange& other)
            {
                // 根据起始位置排序
                return m_Start <=> other.m_Start;
            }
            bool operator==(const AllocationRange& other) const = default;
        };

    public:
        LinearAllocator() = default;
        LinearAllocator(uint64_t maxSize)
            :m_Capacity(maxSize) {
            m_FreeList.emplace_back(0, maxSize);
        }

        uint64_t Allocate(uint64_t size, uint32_t alignment = 0)
        {
            if(size == 0) return InvalidAllocOffset;
            size = Math::Align(size, uint64_t(alignment));

            for(auto it = m_FreeList.begin(); it != m_FreeList.end();){
                if(auto [start, rangeSize] = *it; size <= rangeSize){
                    it = m_FreeList.erase(it);
                    if(size < rangeSize){   //插入后不破坏有序性
                        it = m_FreeList.emplace(it, start + size, rangeSize - size);
                    }
                    return start;
                }
                else{
                    ++it;
                }
            }
            return InvalidAllocOffset;
        }

        bool Deallocate(uint64_t start, uint64_t size)
        {
            if(start + size > m_Capacity || size > m_Capacity) return false;
            if(size == 0) return true;

            AllocationRange newRange{start, size};
            // 获取当前区间的下一个空闲区间
            auto left = std::upper_bound(m_FreeList.begin(), m_FreeList.end(), newRange);
            auto right = left;
            if (left != m_FreeList.begin()) {
                --left;
            }
            
            if(left != m_FreeList.end()){
                if (!(left->m_Start >= (newRange.m_Start + newRange.m_Size) ||
                    newRange.m_Start >= (left->m_Start + left->m_Size))) 
                    return false;

                // 左侧合并
                if(left->m_Start + left->m_Size == newRange.m_Start){
                    newRange.m_Start = left->m_Start;
                    newRange.m_Size += left->m_Size;
                    left = m_FreeList.erase(left);
                }
                // 右侧合并
                if(newRange.m_Start + newRange.m_Size == right->m_Start){
                    newRange.m_Size += right->m_Size;
                    right = m_FreeList.erase(right);
                }
            }

            m_FreeList.insert(right, std::move(newRange));
            return true;
        }

        void Resize(uint64_t size)
        {
            if(m_Capacity == size) return;

            // 扩大容量
            if(size > Capacity()){
                if(!m_FreeList.empty() && m_FreeList.back().m_Start + 
                    m_FreeList.back().m_Size == Capacity()){
                    m_FreeList.back().m_Size = size - m_FreeList.back().m_Start;
                }
                else{
                    m_FreeList.emplace_back(Capacity(), size - Capacity());
                }
            }
            else{   // 缩小容量，需要检测缩小的部分是否已回收
                assert(!Full());
                auto& back = m_FreeList.back();
                uint64_t endPos = back.m_Start + back.m_Size;
                assert(endPos == m_Capacity && back.m_Start < size);
                back.m_Size = size - back.m_Start;
            }
            
            m_Capacity = size;
        }

        // 释放所有的空间,容量不变
        void Clear() 
        {
            m_FreeList.clear();
            m_FreeList.emplace_back(0, Capacity());
        }

        inline uint64_t Capacity() const noexcept
        {
            return m_Capacity;
        }

        inline bool Empty() const noexcept
        {
            return m_FreeList.size() == 1 && m_FreeList.front().m_Size == Capacity();
        }

        inline bool Full() const noexcept
        {
            return m_FreeList.empty();
        }

    public:
        inline static constexpr uint64_t InvalidAllocOffset = uint64_t(-1);

    private:
        std::list<AllocationRange> m_FreeList{};
        uint64_t m_Capacity = 0;
    };


} // namespace DSM 

#endif
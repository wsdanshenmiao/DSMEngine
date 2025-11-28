#pragma once
#ifndef __UTILIS_H__
#define __UTILIS_H__

#include <vector>
#include <mutex>
#include <cassert>

#include "Runtime/Platform/PlatformUtils.h"

namespace DSM::Utility {
    template <class T>
    [[nodiscard]] inline std::size_t HashCombine(std::size_t seed, const T& v)
    {
        std::hash<T> hasher;
        return seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // 用于将父类指针转换为子类指针
    template<typename T, typename U>
    inline T CheckedCast(U u)
    {
        static_assert(!std::is_same_v<T, U>, "Redundant CheckedCast");
#if defined(_DEBUG)
        if (u == nullptr) return nullptr;
        T t = dynamic_cast<T>(u);
        if (!t) assert(!"Invalid type cast");
        return t;
#else
        return static_cast<T>(u);
#endif
    }

    
    template<typename T, typename U> 
    [[nodiscard]] bool ArraysAreDifferent(const T& a, const U& b)
    {
        if (a.size() != b.size()) return true;

        for (size_t i = 0; i < size_t(a.size()); i++) {
            if (a[i] != b[i])
                return true;
        }
        return false;
    }

    template<typename T, typename U> 
    [[nodiscard]] uint32_t ArrayDifferenceMask(const T& a, const U& b)
    {
        assert(a.size() <= 32);
        assert(b.size() <= 32);

        if (a.size() != b.size())
            return ~0u;

        uint32_t mask = 0;
        for (uint32_t i = 0; i < uint32_t(a.size()); i++) {
            if (a[i] != b[i])
                mask |= (1 << i);
        }
        return mask;
    }

    class BitSetAllocator
    {
    public:
        explicit BitSetAllocator(const size_t capacity, bool multiThreaded)
            :m_Allocated(capacity), m_MultiThreaded(multiThreaded) {}

        size_t Allocate()
        {
            if(m_MultiThreaded)
                m_Mutex.lock();

            size_t ret = InvalidAllocOffset;

            size_t capacity = GetCapacity();
            for(size_t i = 0; i < capacity; ++i){
                size_t ii = (m_NextAvailable + i) % capacity;
                if(!m_Allocated[ii]){
                    ret = ii;
                    m_NextAvailable = (ii + 1) % capacity;
                    m_Allocated[ii] = true;
                    break;
                }
            }

            if(m_MultiThreaded)
                m_Mutex.unlock();

            return ret;
        }

        bool Release(size_t index)
        {
            if(index < GetCapacity()){
                if(m_MultiThreaded)
                    m_Mutex.lock();
                m_Allocated[index] = false;
                m_NextAvailable = (std::min)(m_NextAvailable, index);
                if(m_MultiThreaded)
                    m_Mutex.unlock();
                    return true;
            }
            return false;
        }

        [[nodiscard]] size_t GetCapacity() const noexcept { return m_Allocated.size(); }


    public:
        constexpr static size_t InvalidAllocOffset = size_t(-1);

    private:
        std::vector<bool> m_Allocated;
        size_t m_NextAvailable = 0;
        std::mutex m_Mutex{};
        bool m_MultiThreaded{};
    };

} // namespace DSM::Utility 


#endif
#pragma once
#ifndef __UTILIS_H__
#define __UTILIS_H__

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <vector>
#include <mutex>

namespace DSM::Utility {


    inline std::wstring UTF8ToWString(const std::string& str) 
    {
        if (str.empty()) return L"";

        // 计算需要的宽字符数
        int wcharCount = MultiByteToWideChar(
            CP_UTF8,            // 源字符串是 UTF-8
            0,                  // 无特殊标志
            str.c_str(),        // 源字符串
            (int)str.size(),    // 字符串长度（-1 表示自动计算，但这里手动传入大小）
            nullptr,            // 不接收输出，仅计算缓冲区大小
            0                   // 输出缓冲区大小（0 表示仅计算）
        );

        if (wcharCount == 0) {
            return L"";
        }

        std::wstring wstr(wcharCount, 0);

        // 实际转换
        MultiByteToWideChar(
            CP_UTF8,
            0,
            str.c_str(),
            (int)str.size(),
            &wstr[0],          // 输出缓冲区
            wcharCount
        );

        return wstr;
    }

    inline std::string WStringToUTF8(const std::wstring& wstr)
    {
        if (wstr.empty()) return "";

        // 计算需要的字节数
        int byteCount = WideCharToMultiByte(
            CP_UTF8,            // 目标编码是 UTF-8
            0,                  // 无特殊标志
            wstr.c_str(),       // 源宽字符串
            (int)wstr.size(),   // 宽字符数
            nullptr,            // 不接收输出，仅计算缓冲区大小
            0,                  // 输出缓冲区大小（0 表示仅计算）
            nullptr,            // 默认字符（不可转换时使用）
            nullptr             // 是否使用了默认字符
        );

        if (byteCount == 0) {
            return "";
        }

        // 分配足够的空间（+1 用于 null 终止符）
        std::string str(byteCount, 0);

        // 实际转换
        WideCharToMultiByte(
            CP_UTF8,
            0,
            wstr.c_str(),
            (int)wstr.size(),
            &str[0],            // 输出缓冲区
            byteCount,
            nullptr,
            nullptr
        );

        return str;
    }

    template <class T>
    [[nodiscard]] inline std::size_t HashCombine(std::size_t seed, const T& v)
    {
        std::hash<T> hasher;
        return seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template<typename T> 
    inline T Align(T size, T alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
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
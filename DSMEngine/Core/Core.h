#pragma once
#ifndef __CORE_H__
#define __CORE_H__

#include <format>
#include "PlatformDetection.h"
#include "Log.h"

#if defined(DSM_PLATFORM_WINDOWS)
    #include <Windows.h>
#endif

#define BIT(x) (1 << x)

#define DSM_ASSERT( isFalse, ... ) \
    if (!(bool)(isFalse)) { \
        auto error = std::format("\nAssertion failed in {} @ {}\n", __FILE__, __LINE__); \
        error += std::format("\'{}\' is false\n", isFalse);   \
        error += __VA_ARGS__;   \
        error += "\n";  \
        DSM_ERROR(error);   \
        __debugbreak(); \
    }


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
} // namespace DSM::Utility 


#endif
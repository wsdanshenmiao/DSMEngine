#pragma once
#ifndef __UTILITY_H__
#define __UTILITY_H__

#define NOMINMAX

#include <string>
#include <string_view>
#include <format>
#include <Windows.h>


namespace DSM::Utility {
    template<typename... Args>
    inline void Print(const std::string_view format, Args&&... args)
    {
        auto formatArgs{std::make_format_args(args...)};
        std::string outStr{std::vformat(format, formatArgs)};
        fputs(outStr.c_str(), stdout);
    }
    template<typename... Args>
    inline void Print(const std::wstring_view format, Args&&... args)
    {
        auto formatArgs{std::make_wformat_args(args...)};
        std::wstring outStr{std::vformat(format, formatArgs)};
        fputws(outStr.c_str(), stdout);
    }

    template<typename... Args>
    inline void PrintSubMessage(const std::string_view format, Args&&... args)
    {
        Print("--> ");
        Print(format, std::forward<Args>(args)...);
        Print("\n");
    }
    template<typename... Args>
    inline void PrintSubMessage(const std::wstring_view format, Args&&... args)
    {
        Print("--> ");
        Print(format, std::forward<Args>(args)...);
        Print("\n");
    }
 
    inline void PrintSubMessage(){}
    
    // 判断是否使用控制台程序
#if defined(_CONSOLE)
    inline void Print(const char* msg) { Print(msg);}
    inline void Print(const wchar_t* msg) { Print(msg);}
#else
    inline void Print( const char* msg ) { OutputDebugStringA(msg); }
    inline void Print( const wchar_t* msg ) { OutputDebugString(msg); }
#endif


    template <typename T> 
    inline constexpr T AlignUp( T value, size_t alignment ) noexcept
    {
        if (alignment == 0 || alignment == 1) return value;
        else return (T)(((size_t)value + (alignment - 1)) & ~(alignment - 1));
    }



    inline constexpr std::uint64_t INVALID_ALLOC_OFFSET = std::numeric_limits<std::uint64_t>::max();
    
    
}

#endif
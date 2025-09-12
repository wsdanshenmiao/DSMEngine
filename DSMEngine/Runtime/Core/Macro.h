#pragma once
#ifndef __MACRO_H__
#define __MACRO_H__

#include <format>
#include "Runtime/Core/PlatformDetection.h"
#include "Runtime/Core/Global/GlobalContext.h"
#include "Runtime/Core/LogSystem.h"

#if defined(DSM_PLATFORM_WINDOWS)
    #include <Windows.h>
#endif

#define BIT(x) (1 << x)

// Core log macros
#define DSM_CORE_DEBUG(...)    g_GlobalContext.loggerSystem->GetCoreLogger()->debug(__VA_ARGS__)
#define DSM_CORE_TRACE(...)    g_GlobalContext.loggerSystem->GetCoreLogger()->trace(__VA_ARGS__)
#define DSM_CORE_INFO(...)     g_GlobalContext.loggerSystem->GetCoreLogger()->info(__VA_ARGS__)
#define DSM_CORE_WARN(...)     g_GlobalContext.loggerSystem->GetCoreLogger()->warn(__VA_ARGS__)
#define DSM_CORE_ERROR(...)    g_GlobalContext.loggerSystem->GetCoreLogger()->error(__VA_ARGS__)
#define DSM_CORE_CRITICAL(...) g_GlobalContext.loggerSystem->GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define DSM_DEBUG(...)         g_GlobalContext.loggerSystem->GetClientLogger()->debug(__VA_ARGS__)
#define DSM_TRACE(...)         g_GlobalContext.loggerSystem->GetClientLogger()->trace(__VA_ARGS__)
#define DSM_INFO(...)          g_GlobalContext.loggerSystem->GetClientLogger()->info(__VA_ARGS__)
#define DSM_WARN(...)          g_GlobalContext.loggerSystem->GetClientLogger()->warn(__VA_ARGS__)
#define DSM_ERROR(...)         g_GlobalContext.loggerSystem->GetClientLogger()->error(__VA_ARGS__)
#define DSM_CRITICAL(...)      g_GlobalContext.loggerSystem->GetClientLogger()->critical(__VA_ARGS__)

#define DSM_ASSERT( isFalse, ... ) \
    if (!(bool)(isFalse)) { \
        auto error = std::format("\nAssertion failed in {} @ {}\n", __FILE__, __LINE__); \
        error += std::format("\'{}\' is false\n", isFalse);   \
        __VA_OPT__(error += __VA_ARGS__;)   \
        error += "\n";  \
        DSM_ERROR(error);   \
        __debugbreak(); \
    }

#define DSM_CORE_ASSERT( isFalse, ... ) \
    if (!(bool)(isFalse)) { \
        auto error = std::format("\nAssertion failed in {} @ {}\n", __FILE__, __LINE__); \
        error += std::format("\'{}\' is false\n", isFalse);   \
        __VA_OPT__(error += __VA_ARGS__;)   \
        error += "\n";  \
        DSM_CORE_ERROR(error);   \
        __debugbreak(); \
    }



#endif
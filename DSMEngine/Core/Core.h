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
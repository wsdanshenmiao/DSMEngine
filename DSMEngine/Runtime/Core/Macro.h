#pragma once
#ifndef __MACRO_H__
#define __MACRO_H__

#include <format>
#include "Runtime/Core/PlatformDetection.h"
#include "Runtime/Core/LogSystem.h"
#include "Runtime/DSMEngine.h"

#define BIT(x) (1 << x)

// Core log macros
#define DSM_CORE_DEBUG(...)    DSM::DSMEngine::sm_GlobalContext.loggerSystem->CoreLog(DSM::LogSystem::Debug, __VA_ARGS__)
#define DSM_CORE_TRACE(...)    DSM::DSMEngine::sm_GlobalContext.loggerSystem->CoreLog(DSM::LogSystem::Trace, __VA_ARGS__)
#define DSM_CORE_INFO(...)     DSM::DSMEngine::sm_GlobalContext.loggerSystem->CoreLog(DSM::LogSystem::Info, __VA_ARGS__)
#define DSM_CORE_WARN(...)     DSM::DSMEngine::sm_GlobalContext.loggerSystem->CoreLog(DSM::LogSystem::Warn, __VA_ARGS__)
#define DSM_CORE_ERROR(...)    DSM::DSMEngine::sm_GlobalContext.loggerSystem->CoreLog(DSM::LogSystem::Error, __VA_ARGS__)
#define DSM_CORE_CRITICAL(...) DSM::DSMEngine::sm_GlobalContext.loggerSystem->CoreLog(DSM::LogSystem::Fatal, __VA_ARGS__)

// Client log macros
#define DSM_DEBUG(...)         DSM::DSMEngine::sm_GlobalContext.loggerSystem->Log(DSM::LogSystem::Debug, __VA_ARGS__)
#define DSM_TRACE(...)         DSM::DSMEngine::sm_GlobalContext.loggerSystem->Log(DSM::LogSystem::Trace, __VA_ARGS__)
#define DSM_INFO(...)          DSM::DSMEngine::sm_GlobalContext.loggerSystem->Log(DSM::LogSystem::Info, __VA_ARGS__)
#define DSM_WARN(...)          DSM::DSMEngine::sm_GlobalContext.loggerSystem->Log(DSM::LogSystem::Warn, __VA_ARGS__)
#define DSM_ERROR(...)         DSM::DSMEngine::sm_GlobalContext.loggerSystem->Log(DSM::LogSystem::Error, __VA_ARGS__)
#define DSM_CRITICAL(...)      DSM::DSMEngine::sm_GlobalContext.loggerSystem->Log(DSM::LogSystem::Fatal, __VA_ARGS__)
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
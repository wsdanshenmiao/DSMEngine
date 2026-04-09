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

#define DSM_ASSERT_IMPL(isFalse, message, logMacro) \
    do { \
        if (!(bool)(isFalse)) { \
            auto error = std::format("\nAssertion failed in {} @ {}\n", __FILE__, __LINE__); \
            error += std::format("\'{}\' is false\n", #isFalse); \
            error += (message); \
            error += "\n"; \
            logMacro(error); \
            __debugbreak(); \
        } \
    } while (0)

#define DSM_GET_ASSERT_MACRO(_1, _2, NAME, ...) NAME

#define DSM_ASSERT_1(isFalse) DSM_ASSERT_IMPL(isFalse, "", DSM_ERROR)
#define DSM_ASSERT_2(isFalse, message) DSM_ASSERT_IMPL(isFalse, message, DSM_ERROR)
#define DSM_ASSERT(...) DSM_GET_ASSERT_MACRO(__VA_ARGS__, DSM_ASSERT_2, DSM_ASSERT_1)(__VA_ARGS__)

#define DSM_CORE_ASSERT_1(isFalse) DSM_ASSERT_IMPL(isFalse, "", DSM_CORE_ERROR)
#define DSM_CORE_ASSERT_2(isFalse, message) DSM_ASSERT_IMPL(isFalse, message, DSM_CORE_ERROR)
#define DSM_CORE_ASSERT(...) DSM_GET_ASSERT_MACRO(__VA_ARGS__, DSM_CORE_ASSERT_2, DSM_CORE_ASSERT_1)(__VA_ARGS__)



#endif
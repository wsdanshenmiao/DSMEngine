#pragma once
#ifndef __MACRO_H__
#define __MACRO_H__

#include <format>
#include <source_location>
#include <string_view>
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

namespace DSM::Detail {

    enum class AssertLogTarget
    {
        Core,
        Client
    };

    constexpr std::string_view GetAssertionMessage() noexcept
    {
        return {};
    }

    constexpr std::string_view GetAssertionMessage(std::string_view message) noexcept
    {
        return message;
    }

    inline void ReportAssertionFailure(
        AssertLogTarget target,
        std::string_view expression,
        std::string_view message,
        std::source_location location = std::source_location::current())
    {
        auto error = std::format(
            "\nAssertion failed in {} @ {}\n'{}' is false\n{}\n",
            location.file_name(), location.line(), expression, message);

        if (target == AssertLogTarget::Core) {
            DSMEngine::sm_GlobalContext.loggerSystem->CoreLog(LogSystem::Error, error);
        }
        else {
            DSMEngine::sm_GlobalContext.loggerSystem->Log(LogSystem::Error, error);
        }

        __debugbreak();
    }

}

#define DSM_ASSERT_IMPL(condition, message, target) \
    do { \
        if (!static_cast<bool>(condition)) { \
            ::DSM::Detail::ReportAssertionFailure(target, #condition, message); \
        } \
    } while (false)

#define DSM_ASSERT(condition, ...) \
    DSM_ASSERT_IMPL(condition, ::DSM::Detail::GetAssertionMessage(__VA_ARGS__), \
        ::DSM::Detail::AssertLogTarget::Client)

#define DSM_CORE_ASSERT(condition, ...) \
    DSM_ASSERT_IMPL(condition, ::DSM::Detail::GetAssertionMessage(__VA_ARGS__), \
        ::DSM::Detail::AssertLogTarget::Core)



#endif

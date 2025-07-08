#pragma once
#ifndef __LOG_H__
#define __LOG_H__


#include "spdlog/spdlog.h"

namespace DSM {
    class Log
    {
    public:
        static void Init();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return sm_CoreLogger; }
        static std::shared_ptr<spdlog::logger>& GetClientLogger() { return sm_ClientLogger; }

    private:
        inline static std::shared_ptr<spdlog::logger> sm_CoreLogger = nullptr;
        inline static std::shared_ptr<spdlog::logger> sm_ClientLogger = nullptr;
    };

} // namespace DSM 


// Core log macros
#define DSM_CORE_TRACE(...)    DSM::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define DSM_CORE_INFO(...)     DSM::Log::GetCoreLogger()->info(__VA_ARGS__)
#define DSM_CORE_WARN(...)     DSM::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define DSM_CORE_ERROR(...)    DSM::Log::GetCoreLogger()->error(__VA_ARGS__)
#define DSM_CORE_CRITICAL(...) DSM::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define DSM_TRACE(...)         DSM::Log::GetClientLogger()->trace(__VA_ARGS__)
#define DSM_INFO(...)          DSM::Log::GetClientLogger()->info(__VA_ARGS__)
#define DSM_WARN(...)          DSM::Log::GetClientLogger()->warn(__VA_ARGS__)
#define DSM_ERROR(...)         DSM::Log::GetClientLogger()->error(__VA_ARGS__)
#define DSM_CRITICAL(...)      DSM::Log::GetClientLogger()->critical(__VA_ARGS__)


#endif
#pragma once
#ifndef __LOGSYSTEM_H__
#define __LOGSYSTEM_H__

#include "spdlog/spdlog.h"

namespace DSM {
    class LogSystem
    {
    public:
        enum LogLevel
        {
            Debug,
            Trace,
            Info,
            Warn,
            Error,
            Fatal,
            Count
        };

        using LogFunc = std::function<void(LogLevel, const std::string&)>;

    public:
        LogSystem();

        void SetLogFunc(LogFunc&& logFunc) { m_LogFunc = std::forward<LogFunc>(logFunc); }

        template<typename... Args>
        void CoreLog(LogLevel level, std::string_view fmt, Args&&... args)
        {
            std::string text;
            if constexpr (sizeof...(args) > 0) {
                text = std::vformat(fmt, std::make_format_args(args...));
            } else {
                text = std::string(fmt);
            }
            Log(m_CoreLogger, level, text);
            if(m_LogFunc != nullptr){
                m_LogFunc(level, text);
            }
        }

        template<typename... Args>
        void Log(LogLevel level, std::string_view fmt, Args&&... args)
        {
            std::string text;
            if constexpr (sizeof...(args) > 0) {
                text = std::vformat(fmt, std::make_format_args(args...));
            }
            else {
                text = std::string(fmt);
            }
            Log(m_ClientLogger, level, text);
            if(m_LogFunc != nullptr){
                m_LogFunc(level, text);
            }
        }

        std::shared_ptr<spdlog::logger> GetCoreLogger() { return m_CoreLogger; }
        std::shared_ptr<spdlog::logger> GetClientLogger() { return m_ClientLogger; }

    private:
        void Log(std::shared_ptr<spdlog::logger> logger, LogLevel level, const std::string& msg);

    private:
        std::shared_ptr<spdlog::logger> m_CoreLogger = nullptr;
        std::shared_ptr<spdlog::logger> m_ClientLogger = nullptr;

        LogFunc m_LogFunc{};
    };

} // namespace DSM 



#endif
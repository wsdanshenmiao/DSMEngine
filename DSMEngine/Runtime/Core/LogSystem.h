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
            Fatal
        };

    public:
        LogSystem();

        template<typename... Args>
        void CoreLog(LogLevel level, Args&&... args)
        {
            Log(m_CoreLogger, level, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void Log(LogLevel level, Args&&... args)
        {
            Log(m_ClientLogger, level, std::forward<Args>(args)...);
        }

        std::shared_ptr<spdlog::logger> GetCoreLogger() { return m_CoreLogger; }
        std::shared_ptr<spdlog::logger> GetClientLogger() { return m_ClientLogger; }

    private:
        template<typename... Args>
        void Log(std::shared_ptr<spdlog::logger> logger, LogLevel level, Args&&... args)
        {
            switch (level) {
            case LogLevel::Debug:
                logger->debug(std::forward<Args>(args)...);
                break;
            case LogLevel::Trace:
                logger->trace(std::forward<Args>(args)...);
                break;
            case LogLevel::Info:
                logger->info(std::forward<Args>(args)...);
                break;
            case LogLevel::Warn:
                logger->warn(std::forward<Args>(args)...);
                break;
            case LogLevel::Error:
                logger->error(std::forward<Args>(args)...);
                break;
            case LogLevel::Fatal:
                logger->critical(std::forward<Args>(args)...);
                break;
            }
        }

    private:
        std::shared_ptr<spdlog::logger> m_CoreLogger = nullptr;
        std::shared_ptr<spdlog::logger> m_ClientLogger = nullptr;
    };

} // namespace DSM 



#endif
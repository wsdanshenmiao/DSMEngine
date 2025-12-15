#include "LogSystem.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "LogSystem.h"


namespace DSM {
    LogSystem::LogSystem()
    {
        spdlog::set_pattern("%^[%T] %n: %v%$");
        
        m_CoreLogger = spdlog::stdout_color_mt("DSMEngine");
        m_CoreLogger->set_level(spdlog::level::trace);

        m_ClientLogger = spdlog::stdout_color_mt("App");
        m_ClientLogger->set_level(spdlog::level::trace);
    }

    void LogSystem::Log(std::shared_ptr<spdlog::logger> logger, LogLevel level, const std::string &msg)
    {
        switch (level) {
        case LogLevel::Debug: logger->debug(msg); break;
        case LogLevel::Trace: logger->trace(msg); break;
        case LogLevel::Info: logger->info(msg); break;
        case LogLevel::Warn: logger->warn(msg); break;
        case LogLevel::Error: logger->error(msg); break;
        case LogLevel::Fatal: logger->critical(msg); break;
        }
    }

} // namespace DSM
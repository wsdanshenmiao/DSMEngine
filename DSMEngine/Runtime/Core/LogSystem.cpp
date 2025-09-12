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

} // namespace DSM
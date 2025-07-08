#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"


namespace DSM {
    void Log::Init()
    {
        spdlog::set_pattern("%^[%T] %n: %v%$");
        
        sm_CoreLogger = spdlog::stdout_color_mt("DSMEngine");
        sm_CoreLogger->set_level(spdlog::level::trace);

        sm_ClientLogger = spdlog::stdout_color_mt("App");
        sm_ClientLogger->set_level(spdlog::level::trace);
    }

} // namespace DSM 
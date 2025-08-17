#pragma once
#ifndef __ENTERPINT_H__
#define __ENTERPINT_H__

#include "Window.h"

#if defined(DSM_PLATFORM_WINDOWS)
#include "Platform/Windows/WindowsWindow.h"

int WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    
    DSM::Log::Init();
    DSM_CORE_WARN("Initialized Log");
    auto& app = DSM::Application::Create();
    app.Run();
    DSM::Application::ShutDown();
    
    return 0;
}

#endif

#endif
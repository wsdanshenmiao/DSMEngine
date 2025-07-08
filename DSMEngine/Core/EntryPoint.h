#pragma once
#ifndef __ENTERPINT_H__
#define __ENTERPINT_H__

#include "Window.h"

#if defined(DSM_PLATFORM_WINDOWS)
#include "Platform/Windows/WindowsWindow.h"

extern DSM::Application* DSM::CreateApplication(); 

int WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    DSM::Log::Init();
    DSM_CORE_WARN("Initialized Log");
    DSM::WindowsWindowProps winProps;
    winProps.m_Width = 800;
    winProps.m_Height = 800;
    winProps.m_hInstance = hInstance;
    DSM::Window* window = DSM::Window::Create(winProps);
    auto app = DSM::CreateApplication();
    app->Run();
    delete app;
    return 0;
}

#endif

#endif
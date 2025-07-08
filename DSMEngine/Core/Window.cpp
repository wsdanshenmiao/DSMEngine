#include "Window.h"
#include "Core.h"

#if defined(DSM_PLATFORM_WINDOWS)
    #include "Platform/Windows/WindowsWindow.h"
#endif

namespace DSM {

    Window *Window::Create(const WindowProps &winProps)
    {
    #if defined(DSM_PLATFORM_WINDOWS)
        return new WindowsWindow(winProps);
    #else
        return nullptr;
    #endif
    }



} // namespace DSM 
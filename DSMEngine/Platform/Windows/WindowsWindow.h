#ifndef __WINDOWSWINDOW_H__
#define __WINDOWSWINDOW_H__

#include <Windows.h>
#include "Core/Window.h"

namespace DSM {
    struct WindowsWindowProps : public WindowProps
    {
        HINSTANCE m_hInstance;
    };


    class WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowProps& winProps);
        ~WindowsWindow() = default;

        void OnUpdate() override;

        inline uint32_t GetWidth() const override { return m_Width; };
        inline uint32_t GetHeight() const override { return m_Height; };

        inline void SetEventCallback(const EventCallbackFunc& func) override { m_Callback = func; };
        
        void* GetNativeWindow() const override;

    private:
        uint32_t m_Width, m_Height;
        EventCallbackFunc m_Callback;
        HWND m_Handle;
    };

} // namespace DSM 


#endif
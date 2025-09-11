#ifndef __WINDOWSWINDOW_H__
#define __WINDOWSWINDOW_H__

#include "Core/Window.h"

struct GLFWwindow;

namespace DSM {

    class WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowProps& winProps);
        virtual ~WindowsWindow();

        void OnUpdate() override;

        inline uint32_t GetWidth() const override { return m_Desc.width; }
        inline uint32_t GetHeight() const override { return m_Desc.height; }
        inline const std::string& GetTitle() const override { return m_Desc.title; }

        void SetTitle(const std::string& title) override;

        inline void SetEventCallback(const EventCallbackFunc& func) override { m_Desc.callback = func; };
        void SetVSync(bool enabled) override;
		inline bool IsVSync() const override { return m_Desc.VSync; };
        
        inline void* GetNativeWindow() const override { return m_Window; }

    private:
        struct WindowData
        {
            uint32_t width, height;
            bool VSync = false;
            EventCallbackFunc callback;
            std::string title;
        } m_Desc;

        GLFWwindow* m_Window = nullptr;
    };

} // namespace DSM 


#endif
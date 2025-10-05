#pragma once
#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <string>
#include <functional>

struct GLFWwindow;

namespace DSM{
    class Event;

    struct WindowProps
    {
        std::string m_Title = "DSMEngine";
        uint32_t m_Width = 1600;
        uint32_t m_Height = 1024;
    };

    class Window
    {
    public:
        using EventCallbackFunc = std::function<void(Event&)>;
        Window(const WindowProps& desc);
        ~Window();
        void Update();

        uint32_t GetWidth() const { return m_Desc.width; }
        uint32_t GetHeight() const { return m_Desc.height; }
        const std::string& GetTitle() const { return m_Desc.title; }

        void SetTitle(const std::string& title);

        void SetEventCallback(const EventCallbackFunc& func) { m_Desc.callback = func;}
        // 垂直同步
		void SetVSync(bool enabled);
		bool IsVSync() const { return m_Desc.VSync; }
        GLFWwindow* GetNativeWindow() const { return m_Window; }

    private:
        GLFWwindow* m_Window;
        struct WindowData
        {
            uint32_t width, height;
            bool VSync = false;
            EventCallbackFunc callback;
            std::string title;
        } m_Desc;
    };


}


#endif
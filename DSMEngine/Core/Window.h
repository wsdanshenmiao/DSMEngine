#pragma once
#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <string>
#include <functional>
#include "Event/Event.h"

namespace DSM{
    struct WindowProps
    {
        virtual ~WindowProps() = default;
        std::string m_Title = "DSMEngine";
        uint32_t m_Width = 1280;
        uint32_t m_Height = 720;
    };

    class Window
    {
    public:
        using EventCallbackFunc = std::function<void(Event&)>;

        virtual ~Window() = default;

        virtual void OnUpdate() = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual void SetEventCallback(const EventCallbackFunc& func) = 0;
        // 垂直同步
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;
        virtual void* GetNativeWindow() const = 0;

        static Window* Create(const WindowProps& winProps);
    };


}


#endif
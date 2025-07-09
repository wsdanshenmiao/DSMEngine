#pragma once
#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#include <memory>
#include "Core.h"
#include "Window.h"
#include "LayerStack.h"

namespace DSM {
    class WindowResizeEvent;
    class WindowCloseEvent;
    
    class Application
    {
    public:
        Application();

        void Run();

        bool OnWindowResize(WindowResizeEvent& event);
        bool OnWindowClose(WindowCloseEvent& event);

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

    private:
        std::unique_ptr<Window> m_Window{};
        LayerStack m_LayerStack{};
        bool m_Running = true;
    };


    Application* CreateApplication();


} // namespace DSM 

#endif
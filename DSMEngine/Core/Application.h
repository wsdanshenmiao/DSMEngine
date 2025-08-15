#pragma once
#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#include <memory>
#include "Core.h"
#include "Window.h"
#include "LayerStack.h"
#include "GUI/ImguiLayer.h"
#include "Utils/Singleton.h"


namespace DSM {
    class WindowResizeEvent;
    class WindowCloseEvent;
    class Application;


    Application* CreateApplication();

    class Application
    {
    public:
        Application();
        virtual ~Application() = default;

        void Run();

        bool OnWindowResize(WindowResizeEvent& event);
        bool OnWindowClose(WindowCloseEvent& event);

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        Window& GetWindow() { return *m_Window; }

    public:
        static void Create() { std::call_once(m_Initialized, []() { m_Instance.reset(CreateApplication()); }); }

        static Application& GetInstance() 
        { 
            DSM_CORE_ASSERT(m_Instance != nullptr, "Application should be explicit initialized"); 
            return *m_Instance; 
        }

    protected:
        inline static std::once_flag m_Initialized{};
        inline static std::unique_ptr<Application> m_Instance{};

    protected:
        std::unique_ptr<Window> m_Window{};
        LayerStack m_LayerStack{};
        bool m_Running = true;
        std::unique_ptr<ImguiLayer> m_ImguiLayer;
    };


} // namespace DSM 

#endif
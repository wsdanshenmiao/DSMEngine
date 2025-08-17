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
    class Renderer;

    Application* CreateApplication();

    class Application
    {
    public:
        Application();
        virtual ~Application();

        void Run();

        bool OnWindowResize(WindowResizeEvent& event);
        bool OnWindowClose(WindowCloseEvent& event);

        void PushLayer(std::shared_ptr<Layer> layer);
        void PushOverlay(std::shared_ptr<Layer> layer);

        Window& GetWindow() { return *m_Window; }
        std::shared_ptr<ImguiLayer> GetImguiLayer() { return m_ImguiLayer; }
        std::shared_ptr<Renderer> GetRenderLayer() { return m_Renderer; }

        static Application& Create();
        static void ShutDown() { m_Instance = nullptr; }

        static Application& GetInstance() 
        { 
            DSM_CORE_ASSERT(m_Instance != nullptr, "Application should be explicit initialized"); 
            return *m_Instance; 
        }

    protected:
        inline static std::unique_ptr<Application> m_Instance{};

        std::unique_ptr<Window> m_Window{};
        LayerStack m_LayerStack{};
        bool m_Running = true;

        std::shared_ptr<ImguiLayer> m_ImguiLayer;
        std::shared_ptr<Renderer> m_Renderer;
    };


} // namespace DSM 

#endif
#include "Application.h"
#include "Event/ApplicationEvent.h"


namespace DSM {
    Application::Application()
    {    
        WindowProps props{};
        m_Window = std::unique_ptr<Window>(Window::Create(props));
        m_Window->SetEventCallback([this](Event& event){
            //DSM_CORE_INFO(event.ToString());
            EventDispatcher dispatcher{event};
            dispatcher.Dispatch<WindowCloseEvent>([this](auto& event) { return this->OnWindowClose(event); });
            dispatcher.Dispatch<WindowResizeEvent>([this](auto& event) { return this->OnWindowResize(event); });

            for(auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it){
                if(event.m_Handled) break;
                (*it)->OnEvent(event);
            }
        });

        // m_ImguiLayer = std::make_unique<ImguiLayer>();
    }

    void Application::Run()
    {
        while (m_Running)
        {
            for(auto& layer : m_LayerStack){
                layer->OnUpdate();
            }
            m_Window->OnUpdate();
        }
    }

    bool Application::OnWindowResize(WindowResizeEvent &event)
    {
        return true;
    }

    bool Application::OnWindowClose(WindowCloseEvent &event)
    {
        m_Running = false;
        return true;
    }

    void Application::PushLayer(Layer* layer)
    {
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PushOverlay(Layer* layer)
    {
        m_LayerStack.PushOverlay(layer);
        layer->OnAttach();
    }



} // namespace DSM
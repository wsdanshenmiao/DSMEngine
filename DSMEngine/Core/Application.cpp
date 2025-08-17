#include "Application.h"
#include "Event/ApplicationEvent.h"
#include "Render/Renderer.h"

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

        RenderParameters renderDesc{};
        renderDesc.enableDebugRuntime = true;
        renderDesc.logBufferLifetime = true;
        renderDesc.window = m_Window.get();
        m_Renderer.reset(Renderer::Create(GraphicsAPI::D3D12, renderDesc));
        m_ImguiLayer = std::make_shared<ImguiLayer>(m_Renderer->GetDevice(), GetWindow());

        PushLayer(m_Renderer);
        PushLayer(m_ImguiLayer);

        m_Renderer->beforePresent = [this](Renderer& renderer, uint32_t frameIndex){
            m_ImguiLayer->Begin();
            for(auto& layer : m_LayerStack){
                layer->OnGUIRender();
            }
            m_ImguiLayer->End(renderer.GetCurrentFramebuffer());
        };
    }

    Application::~Application()
    {
        m_LayerStack = {};
        m_ImguiLayer = nullptr;
        m_Renderer = nullptr;
        m_Window = nullptr;
    }

    void Application::Run()
    {
        while (m_Running) {
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

    void Application::PushLayer(std::shared_ptr<Layer> layer)
    {
        m_LayerStack.PushLayer(layer);
    }

    void Application::PushOverlay(std::shared_ptr<Layer> layer)
    {
        m_LayerStack.PushOverlay(layer);
    }



} // namespace DSM
#include "Renderer.h"
#include "RendererDX12.h"
#include "Runtime/Event/ApplicationEvent.h"

namespace DSM{

    Renderer::Renderer(RenderParameters renderDesc)
    {
        switch (renderDesc.api) {
        case GraphicsAPI::D3D12:
            m_Internal = std::make_unique<RendererDX12>(renderDesc);
            break;
        default:
            break;
        }
    }

    Renderer::~Renderer()
    {
        m_Internal = nullptr;
    }


    void Renderer::OnEvent(Event &event)
    {
        EventDispatcher dispatcher{event};
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& event){
            // 由于交换链改变大小时所有额外的 Buffer 引用都需要释放
            m_Internal->ResizeFramebuffer(event.GetWidth(), event.GetHeight());
            if(m_RenderPipeline != nullptr){
                m_RenderPipeline->OnResize(event.GetWidth(), event.GetHeight());
            }
            return true;
        });
    }

    IFramebuffer *Renderer::GetFramebuffer(uint32_t index)
    {
        return index < m_Internal->swapChainFramebuffers.size() ? m_Internal->swapChainFramebuffers[index] : nullptr;
    }

    void Renderer::InitWindowUI(WindowUI *windowUI)
    {
        m_Internal->InitWindowUI(windowUI);
    }

    void Renderer::Render(float deltaTime)
    {
        auto callback = [this](const auto& func){
            if(func != nullptr){
                func(*this, m_Internal->frameIndex);
            }
        };

        // 执行渲染与回调函数
        callback(beforeFrame);
        if(BeginFrame()){
            callback(beforeRender);
            if(m_RenderPipeline != nullptr){
                m_RenderPipeline->Render(*this, deltaTime);
            }
            m_Internal->RenderWindowUI();
            callback(afterRender);

            callback(beforePresent);
            Present();
            callback(afterPresent);
        }

        m_Internal->frameIndex++;
    }

}
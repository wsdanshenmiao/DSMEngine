#include "Renderer.h"
#include "RendererDX12.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Core/Window.h"

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

        OnResize(renderDesc.window->GetWidth(), renderDesc.window->GetHeight());
    }

    Renderer::~Renderer()
    {
        m_Internal = nullptr;
        m_RenderPipeline = nullptr;
    }


    void Renderer::OnEvent(Event &event)
    {
        EventDispatcher dispatcher{event};
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& event){
            const auto& bufferDesc = m_Internal->GetCurrentBackBuffer()->GetDesc();
            if(bufferDesc.width == event.GetWidth() && bufferDesc.height == event.GetHeight())
                return false;

            // 最小为1
            OnResize(std::max(event.GetWidth(), 1u), std::max(event.GetHeight(), 1u));
            return true;
        });
    }

    void Renderer::OnResize(uint32_t width, uint32_t height)
    {
        m_Internal->device->WaitForIdle();

        m_Camera.SetViewPort(Viewport{float(width), float(height)});
        m_Camera.SetFrustum(std::numbers::pi * 0.5f, float(width) / float(height), 0.1f, 50.f);

        // 由于交换链改变大小时所有额外的 Buffer 引用都需要释放
        m_Internal->ResizeFramebuffer(width, height);
        if(m_RenderPipeline != nullptr){
            m_RenderPipeline->OnResize(*this, width, height);
        }
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
            m_Internal->BeginWindowUI();
            if(m_RenderPipeline != nullptr){
                m_RenderPipeline->RenderUI(*this);
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
#include "GraphicsRenderer.h"
#include "RendererDX12.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include "Runtime/Core/InstrumentorMacro.h"

namespace DSM{

    GraphicsRenderer::GraphicsRenderer(RenderParameters renderDesc)
    {
        DSM_CORE_ASSERT(renderDesc.window != nullptr, "RenderParameters.window must be valid");
        switch (renderDesc.api) {
        case GraphicsAPI::D3D12:
            m_Internal = std::make_unique<RendererDX12>(renderDesc);
            break;
        default:
            break;
        }

        DSM_CORE_ASSERT(m_Internal != nullptr, "Unsupported graphics API");
        if (m_Internal == nullptr) {
            return;
        }

        ResizeFrameBuffer(renderDesc.window->GetWidth(), renderDesc.window->GetHeight());
        ResizeRenderTexture(renderDesc.window->GetWidth(), renderDesc.window->GetHeight());
    }

    GraphicsRenderer::~GraphicsRenderer()
    {
        m_RenderPipeline = nullptr;
        m_Internal = nullptr;
    }


    void GraphicsRenderer::OnEvent(Event &event)
    {
        EventDispatcher dispatcher{event};
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& event){
            const auto& bufferDesc = m_Internal->GetCurrentBackBuffer()->GetDesc();
            if(bufferDesc.width == event.GetWidth() && bufferDesc.height == event.GetHeight())
                return false;

            // 最小为1
            const uint32_t clampedWidth = std::max(event.GetWidth(), 1u);
            const uint32_t clampedHeight = std::max(event.GetHeight(), 1u);
            ResizeFrameBuffer(clampedWidth, clampedHeight);
            if(m_RenderPipeline != nullptr){
                m_RenderPipeline->OnResizeFrameBuffer(*this, clampedWidth, clampedHeight);
            }
            return true;
        });
        m_Internal->OnEvent(event);
    }

    void GraphicsRenderer::ResizeRenderTexture(uint32_t width, uint32_t height)
    {
        m_Internal->device->WaitForIdle();

        const uint32_t clampedWidth = std::max(width, 1u);
        const uint32_t clampedHeight = std::max(height, 1u);
        m_Camera.SetViewPort(Viewport{float(clampedWidth), float(clampedHeight)});
        m_Camera.SetFrustum(std::numbers::pi * 0.5f, float(clampedWidth) / float(clampedHeight), 0.1f, 30.f);

        m_Internal->colorTex = m_Internal->device->CreateTexture(TextureDesc()
            .SetWidth(clampedWidth)
            .SetHeight(clampedHeight)
            .SetFormat(GetCurrentBackBuffer()->GetDesc().format)
            .SetClearValue(Color{0.0f, 0.0f, 0.0f, 1.0f})
            .SetInitialState(ResourceStates::RenderTarget)
            .SetIsRenderTarget(true)
            .SetDebugName("Renderer::ColorTexture"));

        // 由于交换链改变大小时所有额外的 Buffer 引用都需要释放
        if(m_RenderPipeline != nullptr){
            m_RenderPipeline->OnResizeRenderTexture(*this, clampedWidth, clampedHeight);
        }
    }

    IFramebuffer *GraphicsRenderer::GetFramebuffer(uint32_t index)
    {
        return index < m_Internal->swapChainFramebuffers.size() ? m_Internal->swapChainFramebuffers[index] : nullptr;
    }

    void GraphicsRenderer::InitWindowUI(WindowUI *windowUI)
    {
        m_Internal->InitWindowUI(windowUI);
    }

    void GraphicsRenderer::DestroyWindowUI()
    {
        m_Internal->DestroyWindowUI();
    }

   void GraphicsRenderer::Render(float deltaTime)
   {
       auto callback = [this](const auto& func){
            if(func != nullptr){
                func(*this, m_Internal->frameIndex);
            }
        };

        // 执行渲染与回调函数
        callback(beforeFrame);
        if(InstrumentationTimer timer{"Begin Frame"}; BeginFrame()){
            timer.Stop();
            callback(beforeRender);
            if(m_RenderPipeline != nullptr){
                PROFILE_SCOPE("Render Scene");
                m_RenderPipeline->Render(*this, deltaTime);
            }
            m_Internal->BeginWindowUI();
            if(m_RenderPipeline != nullptr){
                m_RenderPipeline->RenderUI(*this);
            }
            m_Internal->RenderWindowUI();
            callback(afterRender);

            callback(beforePresent);
            PROFILE_SCOPE("Present");
            Present();
            callback(afterPresent);
        }

        m_Internal->frameIndex++;
    }

}

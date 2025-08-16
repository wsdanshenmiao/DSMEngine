#include "RenderLayer.h"
#include "RenderLayerDX12.h"
#include "Event/ApplicationEvent.h"

namespace DSM{
    
    RenderLayer::RenderLayer(const std::string &name, RenderParameters renderDesc)
        : Layer(name), m_Desc(std::move(renderDesc)) {}

    RenderLayer::~RenderLayer()
    {
        m_SwapChainFramebuffers.clear();
    }


    void RenderLayer::OnEvent(Event &event)
    {
        EventDispatcher dispatcher{event};
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& event){
            // 由于交换链改变大小时所有额外的 Buffer 引用都需要释放
            m_SwapChainFramebuffers.clear();
            
            ResizeSwapChain(event.GetWidth(), event.GetHeight());
            m_SwapChainFramebuffers.resize(GetBackBufferCount());
            
            for(uint32_t i = 0; i < GetBackBufferCount(); ++i){
                m_SwapChainFramebuffers[i] = GetDevice()->CreateFramebuffer(
                    FramebufferDesc().AddColorAttachment(GetBackBuffer(i)));
            }

            for(auto& pass : m_RenderPass){
                pass->ResizeBackBuffer(event.GetWidth(), event.GetHeight());
            }

            return true;
        });
    }

    void RenderLayer::AddRenderPass(IRenderPass* pass)
    {
        m_RenderPass.emplace(pass);
    }

    bool RenderLayer::RemoveRenderPass(IRenderPass *pass)
    {
        if(m_RenderPass.contains(pass)){
            m_RenderPass.erase(pass);
            return true;
        }
        return false;
    }

    IFramebuffer *RenderLayer::GetFramebuffer(uint32_t index)
    {
        return index < m_SwapChainFramebuffers.size() ? m_SwapChainFramebuffers[index] : nullptr;
    }

    RenderLayer *RenderLayer::Create(GraphicsAPI api, const RenderParameters& renderDesc)
    {
        switch (api) {
#if defined(DSM_PLATFORM_WINDOWS)
        case GraphicsAPI::D3D12:{
            return new RenderLayerDX12(renderDesc);
        }
#endif
        default:{
            DSM_CORE_ERROR("Unsupported graphics api ({})", static_cast<std::underlying_type_t<GraphicsAPI>>(api));
            return nullptr;
        }
        }
        return nullptr;
    }

    void RenderLayer::Render()
    {
        auto callback = [this](const auto& func){
            if(func != nullptr){
                func(*this, m_FrameIndex);
            }
        };

        // 执行渲染与回调函数
        callback(beforeFrame);
        if(BeginFrame()){
            callback(beforeFrame);
            for(auto& pass : m_RenderPass){
                pass->Render(this, GetCurrentFramebuffer());
            }
            callback(afterRender);

            callback(beforePresent);
            Present();
            callback(beforePresent);
        }

        m_FrameIndex++;
    }

}
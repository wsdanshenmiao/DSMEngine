#pragma once
#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <set>
#include "Runtime/Graphics/GraphicsCommon.h"
#include "Runtime/Graphics/Device.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Render/Camera/Camera.h"

namespace DSM {
    class Window;
    class Event;
    struct WindowUI;
    class Renderer;
    
    class DefaultMessageCallback : public IMessageCallback
    {
    public:
        void Message(MessageSeverity severity, const char* messageText) override
        {
            std::string msg{messageText};
            switch (severity) {
            case MessageSeverity::Error:
                DSM_CORE_ERROR("{}", msg);
                break;
            case MessageSeverity::Fatal:
                DSM_CORE_CRITICAL("{}", msg);
                break;
            case MessageSeverity::Info:
                DSM_CORE_INFO("{}", msg);
                break;
            case MessageSeverity::Warning:
                DSM_CORE_WARN("{}", msg);
                break;
            default:
                break;
            }
        }
    };

    struct IRenderPipeline
    {
        virtual ~IRenderPipeline() = default;
        virtual void Render(Renderer& renderer, float deltaTime) = 0;
        virtual void RenderUI(Renderer& renderer) = 0;
        virtual void OnResize(Renderer& renderer, uint32_t width, uint32_t height) = 0;
    };

    struct RenderParameters
    {
        GraphicsAPI api = GraphicsAPI::D3D12;
        bool logBufferLifetime = false;
        bool enableDebugLayer = true;
        bool allowModeSwitch = false;
        bool enableDebugRuntime = false;
        bool startFullscreen = false;
        bool vsyncEnabled = false;
        uint32_t swapChainBufferCount = 3;
        Format swapChainFormat = Format::SRGBA8_UNORM;
        uint32_t swapChainSampleCount = 1;
        uint32_t swapChainSampleQuality = 0;
        uint32_t refreshRate = 0;
        Window* window;
        IMessageCallback* callback;
    };

    class Renderer
    {
        friend class RendererDX12;
    public:
        using RenderCallbackFunc = std::function<void(Renderer&, uint32_t)>;

        Renderer(RenderParameters renderDesc);
        ~Renderer();

        // 各个图形后端初始化 UI
        void InitWindowUI(WindowUI* windowUI);

        void SetRenderPipeline(std::unique_ptr<IRenderPipeline> renderPipeline) { m_RenderPipeline = std::move(renderPipeline); }

        void Render(float deltaTime);
        void OnEvent(Event& event);

        void ResizeRenderTexture(uint32_t width, uint32_t height);
        void ResizeFrameBuffer(uint32_t width, uint32_t height) { m_Internal->ResizeFramebuffer(width, height); }

        [[nodiscard]] IDevice* GetDevice() const { return m_Internal->device; }
        [[nodiscard]] GraphicsAPI GetGraphicsAPI() const { return m_Internal->GetGraphicsAPI(); };
        [[nodiscard]] uint32_t GetFrameIndex() const { return m_Internal->frameIndex; }
        
        ITexture* GetCurrentBackBuffer() { return m_Internal->GetCurrentBackBuffer(); }
        ITexture* GetBackBuffer(uint32_t index) { return m_Internal->GetBackBuffer(index); }
        uint32_t GetCurrentBackBufferIndex() { return m_Internal->GetCurrentBackBufferIndex(); }
        uint32_t GetBackBufferCount() { return m_Internal->GetBackBufferCount(); }
        IFramebuffer* GetCurrentFramebuffer() { return GetFramebuffer(GetCurrentBackBufferIndex()); }
        IFramebuffer* GetFramebuffer(uint32_t index);
        ITexture* GetColorTexture() { return m_Internal->colorTex; }

        Camera& GetCamera() noexcept { return m_Camera; }

    protected:
        bool BeginFrame() { return m_Internal->BeginFrame(); }
        void Present() { m_Internal->Present(); }

    public:
        RenderCallbackFunc beforeFrame = nullptr;
        RenderCallbackFunc beforeRender = nullptr;
        RenderCallbackFunc afterRender = nullptr;
        RenderCallbackFunc beforePresent = nullptr;
        RenderCallbackFunc afterPresent = nullptr;

    protected:
        struct IRendererInternal
        {
            DeviceHandle device;
            RenderParameters desc;
            std::vector<FramebufferHandle> swapChainFramebuffers{};
            TextureHandle colorTex;
            uint32_t frameIndex = 1;

            [[nodiscard]] virtual GraphicsAPI GetGraphicsAPI() const = 0;

            virtual ITexture* GetCurrentBackBuffer() = 0;
            virtual ITexture* GetBackBuffer(uint32_t index) = 0;
            virtual uint32_t GetCurrentBackBufferIndex() = 0;
            virtual uint32_t GetBackBufferCount() = 0;
            virtual void ResizeSwapChain(uint32_t width, uint32_t height) = 0;

            virtual void InitWindowUI(WindowUI* windowUI) = 0;
            virtual void BeginWindowUI() = 0;
            virtual void RenderWindowUI() = 0;

            virtual void OnEvent(Event& event) = 0;

            virtual bool BeginFrame() = 0;
            virtual void Present() = 0;

            void ResizeFramebuffer(uint32_t width, uint32_t height)
            {
                swapChainFramebuffers.clear();

                ResizeSwapChain(width, height);
                swapChainFramebuffers.resize(GetBackBufferCount());
                
                for(uint32_t i = 0; i < GetBackBufferCount(); ++i){
                    swapChainFramebuffers[i] = device->CreateFramebuffer(
                        FramebufferDesc().AddColorAttachment(GetBackBuffer(i)));
                }
            }
        };
        std::unique_ptr<IRendererInternal> m_Internal;

        std::unique_ptr<IRenderPipeline> m_RenderPipeline;
        Camera m_Camera;
    };

} // namespace DSM


#endif
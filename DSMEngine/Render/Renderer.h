#pragma once
#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <set>
#include "Core/Layer.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/Device.h"

namespace DSM {
    class IRenderPass;
    class Window;

    class DefaultMessageCallback : public IMessageCallback
    {
    public:
        void Message(MessageSeverity severity, const char* messageText) override
        {
            std::string msg{messageText};
            switch (severity) {
            case MessageSeverity::Error:
                DSM_CORE_ERROR(msg);
                break;
            case MessageSeverity::Fatal:
                DSM_CORE_CRITICAL(msg);
                break;
            case MessageSeverity::Info:
                DSM_CORE_INFO(msg);
                break;
            case MessageSeverity::Warning:
                DSM_CORE_WARN(msg);
                break;
            default:
                break;
            }
        }
    };

    struct RenderParameters
    {
        bool logBufferLifetime = false;
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

    class Renderer : public Layer
    {
    public:
        using RenderCallbackFunc = std::function<void(Renderer&, uint32_t)>;

        Renderer(const std::string& name, RenderParameters renderDesc);
        ~Renderer();

        [[nodiscard]] IDevice* GetDevice() const { return m_Device; }
        [[nodiscard]] virtual GraphicsAPI GetGraphicsAPI() const = 0;
        [[nodiscard]] uint32_t GetFrameIndex() const { return m_FrameIndex; }
        
        void OnUpdate(const CpuTimer& timer) override { Render(timer); }
        void OnEvent(Event& event) override;

        void AddRenderPass(IRenderPass* pass);
        bool RemoveRenderPass(IRenderPass* pass);
        
        virtual ITexture* GetCurrentBackBuffer() = 0;
        virtual ITexture* GetBackBuffer(uint32_t index) = 0;
        virtual uint32_t GetCurrentBackBufferIndex() = 0;
        virtual uint32_t GetBackBufferCount() = 0;
        IFramebuffer* GetCurrentFramebuffer() { return GetFramebuffer(GetCurrentBackBufferIndex()); }
        IFramebuffer* GetFramebuffer(uint32_t index);

        static Renderer* Create(GraphicsAPI api, const RenderParameters& renderDesc);

    protected:
        virtual void ResizeSwapChain(uint32_t width, uint32_t height) = 0;

        void Render(const CpuTimer& timer);
        virtual bool BeginFrame() = 0;
        virtual void Present() = 0;


    public:
        RenderCallbackFunc beforeFrame = nullptr;
        RenderCallbackFunc beforeRender = nullptr;
        RenderCallbackFunc afterRender = nullptr;
        RenderCallbackFunc beforePresent = nullptr;
        RenderCallbackFunc afterPresent = nullptr;

    protected:
        RenderParameters m_Desc;
        DeviceHandle m_Device;

        std::vector<FramebufferHandle> m_SwapChainFramebuffers{};
        std::set<IRenderPass*> m_RenderPass;
        uint32_t m_FrameIndex = 0;   
    };

    struct IRenderPass
    {
        virtual void Render(const CpuTimer& deltaTime, Renderer* renderlayer, IFramebuffer* framebuffer) {}
        virtual void OnResize(uint32_t width, uint32_t height) {}
    };


} // namespace DSM


#endif
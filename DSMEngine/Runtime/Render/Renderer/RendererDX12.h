#pragma once
#ifndef __RENDERER_DX12_H__
#define __RENDERER_DX12_H__


#include <dxgi1_6.h>
#include "Renderer.h"
#include <Runtime/Graphics/D3D12.h>

namespace DSM{
    class RendererDX12 : public Renderer::IRendererInternal
    {
    public:
        RendererDX12(const RenderParameters& renderDesc);
        ~RendererDX12();

        inline GraphicsAPI GetGraphicsAPI() const override { return GraphicsAPI::D3D12; };
        
        ITexture* GetCurrentBackBuffer() override { return m_SwapChainBuffers[GetCurrentBackBufferIndex()]; }
        ITexture* GetBackBuffer(uint32_t index) override { return index < m_SwapChainBuffers.size() ? m_SwapChainBuffers[index] : nullptr; }
        uint32_t GetCurrentBackBufferIndex() override { return m_SwapChain->GetCurrentBackBufferIndex(); }
        uint32_t GetBackBufferCount() override { return m_SwapChainDesc.BufferCount; }

        void InitWindowUI(WindowUI* windowUI) override;
        void BeginWindowUI() override;
        void RenderWindowUI() override;

        void OnEvent(Event& event) override;

    protected:
        void ResizeSwapChain(uint32_t width, uint32_t height) override;

        bool BeginFrame() override;
        void Present() override;

        void CreateRenderTarget(uint32_t width, uint32_t height);

    protected:
        WindowUI* m_WindowUI = nullptr;
        DXGI_SWAP_CHAIN_DESC1 m_SwapChainDesc{};
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_FullScreenDesc{};
        RefPtr<IDXGISwapChain3> m_SwapChain;

        RefPtr<ID3D12Fence> m_FrameFence;
        std::vector<HANDLE> m_FrameFenceEvents{};

        std::vector<TextureHandle> m_SwapChainBuffers{};
        
        DefaultMessageCallback m_Callback{};
        HWND m_hWnd;

        bool m_TearingSupported = false;
    };
}

#endif

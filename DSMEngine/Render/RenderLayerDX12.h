#pragma once
#ifndef __RENDERLAYER_DX12_H__
#define __RENDERLAYER_DX12_H__


#include <dxgi1_6.h>
#include "RenderLayer.h"

namespace DSM{
    class RenderLayerDX12 : public RenderLayer
    {
    public:
        RenderLayerDX12(const RenderParameters& renderDesc);
        ~RenderLayerDX12();

        inline GraphicsAPI GetGraphicsAPI() const override { return GraphicsAPI::D3D12; };
        
        ITexture* GetCurrentBackBuffer() override;
        ITexture* GetBackBuffer(uint32_t index) override;
        uint32_t GetCurrentBackBufferIndex() override;
        uint32_t GetBackBufferCount() override;

    protected:
        void Render();

        void ResizeSwapChain(uint32_t width, uint32_t height) override;

        bool BeginFrame() override;
        void Present() override;

        void CreateRenderTarget();

    protected:
        DXGI_SWAP_CHAIN_DESC1 m_SwapChainDesc{};
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_FullScreenDesc{};
        RefPtr<IDXGISwapChain3> m_SwapChain;
        std::vector<TextureHandle> m_SwapChainBuffers{};
        
        DefaultMessageCallback m_Callback{};
        HWND m_hWnd;

        bool m_TearingSupported = false;
    };
}

#endif

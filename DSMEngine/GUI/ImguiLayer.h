#pragma once
#ifndef __IMGUILAYER_H__
#define __IMGUILAYER_H__

#include <d3d12.h>
#include <wrl/client.h>
#include "Core/Layer.h"

namespace DSM {
    class ImguiLayer : public Layer
    {
    public:
        ImguiLayer(ID3D12Device* device, DXGI_FORMAT rtvFormat, HWND hwnd);

        void OnAttach() override;
        void OnDetach() override;
        void OnEvent(Event& event) override;

    private:
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvHeap{};
        DXGI_FORMAT m_RtvFormat;
        HWND m_HWND;
    };
    
} // namespace DSM 

#endif
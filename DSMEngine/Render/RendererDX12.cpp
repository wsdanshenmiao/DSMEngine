#include "RendererDX12.h"
#include "Graphics/D3D12.h"
#include "Renderer.h"
#include "Event/ApplicationEvent.h"
#include "Core/Window.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace DSM::D3D12;

namespace DSM{
    
    RenderLayerDX12::RenderLayerDX12(const RenderParameters& renderDesc)
        : Renderer("RenderLayer with DirectX12", renderDesc)
    {
        m_Desc.callback = m_Desc.callback == nullptr ? &m_Callback : m_Desc.callback;
        DeviceDesc deviceDesc{};
        deviceDesc.errorCB = m_Desc.callback;
        deviceDesc.logBufferLifetime = true;
        m_Device = CreateDevice(deviceDesc);
        
        GLFWwindow* window = static_cast<GLFWwindow*>(m_Desc.window->GetNativeWindow());

        m_hWnd = glfwGetWin32Window(window);

        RECT clientRect;
        GetClientRect(m_hWnd, &clientRect);
        UINT width = clientRect.right - clientRect.left;
        UINT height = clientRect.bottom - clientRect.top;

        ZeroMemory(&m_SwapChainDesc, sizeof(m_SwapChainDesc));
        m_SwapChainDesc.Width = width;
        m_SwapChainDesc.Height = height;
        m_SwapChainDesc.SampleDesc.Count = m_Desc.swapChainSampleCount;
        m_SwapChainDesc.SampleDesc.Quality = m_Desc.swapChainSampleQuality;
        m_SwapChainDesc.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
        m_SwapChainDesc.BufferCount = m_Desc.swapChainBufferCount;
        m_SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        m_SwapChainDesc.Flags = m_Desc.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

        switch (m_Desc.swapChainFormat) {
        case Format::SRGBA8_UNORM:
            m_SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case Format::SBGRA8_UNORM:
            m_SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        default:
            m_SwapChainDesc.Format = D3D12::ConvertFormat(m_Desc.swapChainFormat);
            break;
        }

        RefPtr<IDXGIFactory2> dxgiFactory = nullptr;
        auto hr = CreateDXGIFactory2(m_Desc.enableDebugRuntime ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to create dxgifactory.");
        
        RefPtr<IDXGIFactory5> pDxgiFactory5;
        if (SUCCEEDED(dxgiFactory->QueryInterface(IID_PPV_ARGS(&pDxgiFactory5)))) {
            BOOL supported = 0;
            if (SUCCEEDED(pDxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported))))
                m_TearingSupported = (supported != 0);
        }

        if (m_TearingSupported) {
            m_SwapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        m_FullScreenDesc = {};
        m_FullScreenDesc.RefreshRate.Numerator = m_Desc.refreshRate;
        m_FullScreenDesc.RefreshRate.Denominator = 1;
        m_FullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        m_FullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        m_FullScreenDesc.Windowed = !m_Desc.startFullscreen;
        
        RefPtr<IDXGISwapChain1> pSwapChain1;
        auto queue = m_Device->GetNativeQueue(ObjectTypes::D3D12_CommandQueue, CommandQueueType::Graphics);
        hr = dxgiFactory->CreateSwapChainForHwnd(queue, m_hWnd, &m_SwapChainDesc, &m_FullScreenDesc, nullptr, &pSwapChain1);
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to create swapchain.");
        hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(m_SwapChain.GetAddressOf()));
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to convert swapchain.");

        CreateRenderTarget();

        WindowResizeEvent resizeEvent{width, height};
        OnEvent(resizeEvent);
    }

    RenderLayerDX12::~RenderLayerDX12()
    {
        // 需要等待GPU处理完所有事件，否则交换链的资源无法正常释放
        m_Device->WaitForIdle();
        m_Device->RunGarbageCollection();
        m_SwapChainBuffers.clear();
    }

    ITexture *RenderLayerDX12::GetCurrentBackBuffer() 
    {
        return m_SwapChainBuffers[GetCurrentBackBufferIndex()];
    }

    ITexture *RenderLayerDX12::GetBackBuffer(uint32_t index)
    {
        return index < m_SwapChainBuffers.size() ? m_SwapChainBuffers[index] : nullptr;
    }

    uint32_t RenderLayerDX12::GetCurrentBackBufferIndex()
    {
        return m_SwapChain->GetCurrentBackBufferIndex();
    }

    uint32_t RenderLayerDX12::GetBackBufferCount()
    {
        return m_SwapChainDesc.BufferCount;
    }

    void RenderLayerDX12::ResizeSwapChain(uint32_t width, uint32_t height)
    {
        if(m_Device != nullptr){
            m_Device->WaitForIdle();
            m_Device->RunGarbageCollection();
        }
        m_SwapChainBuffers.clear();

        auto hr = m_SwapChain->ResizeBuffers(
            m_Desc.swapChainBufferCount, 
            width, height, 
            m_SwapChainDesc.Format,
            m_SwapChainDesc.Flags);

        DSM_CORE_ASSERT(SUCCEEDED(hr), "Resize swapchain Failed");

        CreateRenderTarget();
    }

    bool RenderLayerDX12::BeginFrame()
    {
        return true;
    }
    void RenderLayerDX12::Present()
    {
        UINT presentFlags = 0;
        if (!m_Desc.vsyncEnabled && m_FullScreenDesc.Windowed && m_TearingSupported)
            presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

        auto hr = m_SwapChain->Present(m_Desc.vsyncEnabled ? 1 : 0, presentFlags);
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to present.");
    }
    
    void RenderLayerDX12::CreateRenderTarget()
    {
        m_SwapChainBuffers.resize(m_SwapChainDesc.BufferCount);
        for(uint32_t i = 0; i < GetBackBufferCount(); ++i){
            RefPtr<ID3D12Resource> resource;
            auto hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(resource.GetAddressOf()));
            DSM_CORE_ASSERT(SUCCEEDED(hr), "Get swapchain buffer failed.");
            
            TextureDesc texDesc{};
            texDesc.width = m_SwapChainDesc.Width;
            texDesc.height = m_SwapChainDesc.Height;
            texDesc.format = m_Desc.swapChainFormat;
            texDesc.sampleCount = m_Desc.swapChainSampleCount;
            texDesc.sampleQuality = m_Desc.swapChainSampleQuality;
            texDesc.initialState = ResourceStates::Present;
            texDesc.isRenderTarget = true;
            texDesc.keepInitialState = true;

            m_SwapChainBuffers[i] = m_Device->CreateHandleForNativeTexture(ObjectTypes::D3D12_Resource, resource.Get(), texDesc);
        }
    }
}
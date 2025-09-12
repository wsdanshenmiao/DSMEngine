#include "RendererDX12.h"
#include "Runtime/Graphics/D3D12.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Render/TextureManager.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Render/WindowUI.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_glfw.h>

using namespace DSM::D3D12;

namespace DSM{
    static DescriptorHeapHandle s_DescriptorHeap = nullptr;

    RendererDX12::RendererDX12(const RenderParameters& renderDesc)
    {
        desc = renderDesc;
        desc.callback = desc.callback == nullptr ? &m_Callback : desc.callback;
        DeviceDesc deviceDesc{};
        deviceDesc.errorCB = desc.callback;
        deviceDesc.logBufferLifetime = true;
        device = CreateDevice(deviceDesc);

        TextureManager::Init(device);
        ModelLoader::Init(device);

        GLFWwindow* window = static_cast<GLFWwindow*>(desc.window->GetNativeWindow());

        m_hWnd = glfwGetWin32Window(window);

        RECT clientRect;
        GetClientRect(m_hWnd, &clientRect);
        UINT width = clientRect.right - clientRect.left;
        UINT height = clientRect.bottom - clientRect.top;

        ZeroMemory(&m_SwapChainDesc, sizeof(m_SwapChainDesc));
        m_SwapChainDesc.Width = width;
        m_SwapChainDesc.Height = height;
        m_SwapChainDesc.SampleDesc.Count = desc.swapChainSampleCount;
        m_SwapChainDesc.SampleDesc.Quality = desc.swapChainSampleQuality;
        m_SwapChainDesc.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
        m_SwapChainDesc.BufferCount = desc.swapChainBufferCount;
        m_SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        m_SwapChainDesc.Flags = desc.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

        switch (desc.swapChainFormat) {
        case Format::SRGBA8_UNORM:
            m_SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case Format::SBGRA8_UNORM:
            m_SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        default:
            m_SwapChainDesc.Format = D3D12::ConvertFormat(desc.swapChainFormat);
            break;
        }

        RefPtr<IDXGIFactory2> dxgiFactory = nullptr;
        auto hr = CreateDXGIFactory2(desc.enableDebugRuntime ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
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
        m_FullScreenDesc.RefreshRate.Numerator = desc.refreshRate;
        m_FullScreenDesc.RefreshRate.Denominator = 1;
        m_FullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        m_FullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        m_FullScreenDesc.Windowed = !desc.startFullscreen;
        
        RefPtr<IDXGISwapChain1> pSwapChain1;
        auto queue = device->GetNativeQueue(ObjectTypes::D3D12_CommandQueue, CommandQueueType::Graphics);
        hr = dxgiFactory->CreateSwapChainForHwnd(queue, m_hWnd, &m_SwapChainDesc, &m_FullScreenDesc, nullptr, &pSwapChain1);
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to create swapchain.");
        hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(m_SwapChain.GetAddressOf()));
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to convert swapchain.");

        CreateRenderTarget();

        ResizeFramebuffer(width, height);
    }

    RendererDX12::~RendererDX12()
    {
        TextureManager::Destroy();
        ModelLoader::Destroy();
        if(m_WindowUI != nullptr){
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
        
        // 需要等待GPU处理完所有事件，否则交换链的资源无法正常释放
        device->WaitForIdle();
        device->RunGarbageCollection();
        m_SwapChainBuffers.clear();
    }

    ITexture *RendererDX12::GetCurrentBackBuffer() 
    {
        return m_SwapChainBuffers[GetCurrentBackBufferIndex()];
    }

    ITexture *RendererDX12::GetBackBuffer(uint32_t index)
    {
        return index < m_SwapChainBuffers.size() ? m_SwapChainBuffers[index] : nullptr;
    }

    uint32_t RendererDX12::GetCurrentBackBufferIndex()
    {
        return m_SwapChain->GetCurrentBackBufferIndex();
    }

    uint32_t RendererDX12::GetBackBufferCount()
    {
        return m_SwapChainDesc.BufferCount;
    }

    void RendererDX12::InitWindowUI(WindowUI *windowUI)
    {
        m_WindowUI = windowUI;
        
        if(s_DescriptorHeap == nullptr){
            D3D12::IDevice* backDevice = Utility::CheckedCast<D3D12::IDevice*>(device.Get());
            s_DescriptorHeap = backDevice->CreateDescriptorHeap(D3D12::DescriptorHeapType::ShaderResourceView, 64, true);
        }
        ImGui_ImplDX12_InitInfo init_info{};
        init_info.Device = device->GetNativeObject(ObjectTypes::D3D12_Device);
        init_info.CommandQueue = device->GetNativeQueue(ObjectTypes::D3D12_CommandQueue, CommandQueueType::Graphics);
        init_info.NumFramesInFlight = GetBackBufferCount();
        init_info.RTVFormat = m_SwapChainDesc.Format;
        init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        init_info.SrvDescriptorHeap = s_DescriptorHeap->GetHeap();
        init_info.SrvDescriptorAllocFn = [](auto, auto cpu_handle, auto gpu_handle) { 
            if (s_DescriptorHeap != nullptr) {
                uint32_t index = s_DescriptorHeap->AllocateDescriptor();
                *cpu_handle = s_DescriptorHeap->GetCpuHandleShaderVisible(index);
                *gpu_handle = s_DescriptorHeap->GetGpuHandle(index);
            }
        };
        init_info.SrvDescriptorFreeFn = [](auto, auto cpu_handle, auto gpu_handle){
            if (s_DescriptorHeap != nullptr) {
                s_DescriptorHeap->ReleaseDescriptor(s_DescriptorHeap->GetOffsetOfGpuHandle(gpu_handle.ptr));
            }
        };
        ImGui_ImplDX12_Init(&init_info);
    }

    void RendererDX12::RenderWindowUI()
    {
        if(m_WindowUI == nullptr) 
            return;

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto fb = swapChainFramebuffers[GetCurrentBackBufferIndex()];

        auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("ImGui Command List"));
        cmdList->Open();

        D3D12_CPU_DESCRIPTOR_HANDLE DSV;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> RTVs;
        for(const auto& rt : fb->GetDesc().colorAttachments){
            if(rt.texture == nullptr) continue;
            cmdList->SetTextureState(rt.texture, rt.subresources, ResourceStates::RenderTarget);    
            auto rtv = rt.texture->GetNativeView(ObjectTypes::D3D12_RenderTargetViewDescriptor, rt.format, rt.subresources);
            RTVs.push_back(D3D12_CPU_DESCRIPTOR_HANDLE{rtv.integer});
        }
        cmdList->CommitBarriers();
        if(RTVs.empty()){
            return;
        }

        ID3D12GraphicsCommandList* nativeList = cmdList->GetNativeObject(ObjectTypes::D3D12_GraphicsCommandList);
        nativeList->OMSetRenderTargets(RTVs.size(), RTVs.data(), false, nullptr);
        auto heap = s_DescriptorHeap->GetShaderVisibleHeap();
        nativeList->SetDescriptorHeaps(1, &heap);

        m_WindowUI->Render();
        
		ImGui::Render();

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), nativeList);

        for(const auto& rt : fb->GetDesc().colorAttachments){
            if (rt.texture == nullptr) continue;
            cmdList->SetTextureState(rt.texture, rt.subresources, ResourceStates::Present);
        }
        cmdList->CommitBarriers();

        cmdList->Close();
        device->ExecuteCommandList(cmdList);
    }

    void RendererDX12::ResizeSwapChain(uint32_t width, uint32_t height)
    {
        if (device != nullptr) {
            device->WaitForIdle();
            device->RunGarbageCollection();
        }
        m_SwapChainBuffers.clear();

        auto hr = m_SwapChain->ResizeBuffers(
            desc.swapChainBufferCount, 
            width, height, 
            m_SwapChainDesc.Format,
            m_SwapChainDesc.Flags);

        DSM_CORE_ASSERT(SUCCEEDED(hr), "Resize swapchain Failed");

        CreateRenderTarget();
    }

    bool RendererDX12::BeginFrame()
    {
        return true;
    }
    void RendererDX12::Present()
    {
        UINT presentFlags = 0;
        if (!desc.vsyncEnabled && m_FullScreenDesc.Windowed && m_TearingSupported)
            presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

        auto hr = m_SwapChain->Present(desc.vsyncEnabled ? 1 : 0, presentFlags);
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to present.");
    }
    
    void RendererDX12::CreateRenderTarget()
    {
        m_SwapChainBuffers.resize(m_SwapChainDesc.BufferCount);
        for(uint32_t i = 0; i < GetBackBufferCount(); ++i){
            RefPtr<ID3D12Resource> resource;
            auto hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(resource.GetAddressOf()));
            DSM_CORE_ASSERT(SUCCEEDED(hr), "Get swapchain buffer failed.");
            
            TextureDesc texDesc{};
            texDesc.width = m_SwapChainDesc.Width;
            texDesc.height = m_SwapChainDesc.Height;
            texDesc.format = desc.swapChainFormat;
            texDesc.sampleCount = desc.swapChainSampleCount;
            texDesc.sampleQuality = desc.swapChainSampleQuality;
            texDesc.initialState = ResourceStates::Present;
            texDesc.isRenderTarget = true;
            texDesc.keepInitialState = true;

            m_SwapChainBuffers[i] = device->CreateHandleForNativeTexture(ObjectTypes::D3D12_Resource, resource.Get(), texDesc);
        }
    }
}
#include "RendererDX12.h"
#include "Runtime/Graphics/D3D12.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Render/TextureManager.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Render/WindowUI.h"
#include "Runtime/Core/InstrumentorTimer.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_glfw.h>
#include <ImGuizmo.h>

using namespace DSM::D3D12;

namespace DSM{
    static IDescriptorHeap* s_DescriptorHeap = nullptr;

    RendererDX12::RendererDX12(const RenderParameters& renderDesc)
    {
        desc = renderDesc;
        desc.callback = desc.callback == nullptr ? &m_Callback : desc.callback;
        DeviceDesc deviceDesc{};
        deviceDesc.errorCB = desc.callback;
        deviceDesc.logBufferLifetime = desc.logBufferLifetime;
        deviceDesc.enableDebugLayer = desc.enableDebugLayer;
        device = CreateDevice(deviceDesc);

        TextureManager::Init(device);
        ModelLoader::Init(device);

        GLFWwindow* window = static_cast<GLFWwindow*>(desc.window->GetNativeWindow());

        m_hWnd = glfwGetWin32Window(window);

        UINT width = desc.window->GetWidth();
        UINT height = desc.window->GetHeight();

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

        CreateRenderTarget(width, height);

        ID3D12Device* device12 = device->GetNativeObject(ObjectTypes::D3D12_Device);
        hr = device12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FrameFence));
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to create frame fence.");

        for(UINT bufferIndex = 0; bufferIndex < m_SwapChainDesc.BufferCount; bufferIndex++) {
            m_FrameFenceEvents.push_back( CreateEvent(nullptr, false, true, nullptr) );
        }
    }

    RendererDX12::~RendererDX12()
    {
        TextureManager::Destroy();
        ModelLoader::Destroy();
        s_DescriptorHeap = nullptr;
        
        // 需要等待GPU处理完所有事件，否则交换链的资源无法正常释放
        device->WaitForIdle();
        device->RunGarbageCollection();
        m_SwapChainBuffers.clear();

        for(UINT bufferIndex = 0; bufferIndex < m_SwapChainDesc.BufferCount; bufferIndex++) {
            CloseHandle(m_FrameFenceEvents[bufferIndex]);
        }
        m_FrameFenceEvents.clear();

        m_FrameFence = nullptr;
    }

    void RendererDX12::InitWindowUI(WindowUI *windowUI)
    {
        m_WindowUI = windowUI;

        D3D12::IDevice* backDevice = Utility::CheckedCast<D3D12::IDevice*>(device.Get());
        s_DescriptorHeap = backDevice->GetDescriptorHeap(DescriptorHeapType::ShaderResourceView);

        ImGui_ImplDX12_InitInfo init_info{};
        init_info.Device = device->GetNativeObject(ObjectTypes::D3D12_Device);
        init_info.CommandQueue = device->GetNativeQueue(ObjectTypes::D3D12_CommandQueue, CommandQueueType::Graphics);
        init_info.NumFramesInFlight = GetBackBufferCount();
        init_info.RTVFormat = m_SwapChainDesc.Format;
        init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        init_info.SrvDescriptorHeap = s_DescriptorHeap->GetShaderVisibleHeap();
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

    void RendererDX12::DestroyWindowUI()
    {
        if(m_WindowUI != nullptr){
            ImGui_ImplDX12_Shutdown();
        }
    }

    void RendererDX12::BeginWindowUI()
    {
        if(m_WindowUI == nullptr) 
            return;

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        m_WindowUI->OnGUI();
    }

    void RendererDX12::RenderWindowUI()
    {
        if(m_WindowUI == nullptr) 
            return;

        auto backDevice = Utility::CheckedCast<D3D12::IDevice*>(device.Get());
        // 由于描述符堆会会重新创建
        if(s_DescriptorHeap != backDevice->GetDescriptorHeap(DescriptorHeapType::ShaderResourceView)){
            // 重新创建 Imgui 的后端
            ImGui_ImplDX12_Shutdown();
            InitWindowUI(m_WindowUI);
        }

        auto fb = swapChainFramebuffers[GetCurrentBackBufferIndex()].Get();

        auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("ImGui Command List"));
        cmdList->Open();

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> RTVs{};
        for(const auto& rt : fb->GetDesc().colorAttachments){
            if(rt.texture == nullptr) 
                continue;
            cmdList->SetTextureState(rt.texture, rt.subresources, ResourceStates::RenderTarget);    
            auto rtv = rt.texture->GetNativeView(ObjectTypes::D3D12_RenderTargetViewDescriptor, Format::RGBA8_UNORM, rt.subresources);
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

        cmdList->SetTextureState(colorTex, AllSubresources, ResourceStates::PixelShaderResource);
        cmdList->CommitBarriers();

		ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), nativeList);

        for(const auto& rt : fb->GetDesc().colorAttachments){
            if (rt.texture == nullptr) continue;
            cmdList->SetTextureState(rt.texture, rt.subresources, ResourceStates::Present);
        }

        cmdList->Close();
        device->ExecuteCommandList(cmdList);

        if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable){
            GLFWwindow* currentContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(currentContext);
        }
    }

    void RendererDX12::OnEvent(Event &event)
    {
        if (m_WindowUI == nullptr) {
            return;
        }
        m_WindowUI->OnEvent(event);
    }

    void RendererDX12::ResizeSwapChain(uint32_t width, uint32_t height)
    {
        if (device != nullptr) {
            device->WaitForIdle();
            device->RunGarbageCollection();
        }

        for(auto& event : m_FrameFenceEvents){
            SetEvent(event);
        }

        m_SwapChainBuffers.clear();

        auto hr = m_SwapChain->ResizeBuffers(
            desc.swapChainBufferCount, 
            width, height, 
            m_SwapChainDesc.Format,
            m_SwapChainDesc.Flags);

        DSM_CORE_ASSERT(SUCCEEDED(hr), "Resize swapchain Failed");

        CreateRenderTarget(width, height);
    }

    bool RendererDX12::BeginFrame()
    {
        auto bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
        WaitForSingleObject(m_FrameFenceEvents[bufferIndex], INFINITE);
        
        return true;
    }
    void RendererDX12::Present()
    {
        UINT presentFlags = 0;
        if (!desc.vsyncEnabled && m_FullScreenDesc.Windowed && m_TearingSupported)
            presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

        auto hr = m_SwapChain->Present(desc.vsyncEnabled ? 1 : 0, presentFlags);
        DSM_CORE_ASSERT(SUCCEEDED(hr), "Failed to present.");

        auto bufferIndex = GetCurrentBackBufferIndex();
        ID3D12CommandQueue* graphicsQueue = device->GetNativeObject(ObjectTypes::D3D12_CommandQueue);
        m_FrameFence->SetEventOnCompletion(frameIndex, m_FrameFenceEvents[bufferIndex]);
        graphicsQueue->Signal(m_FrameFence, frameIndex);
    }
    
    void RendererDX12::CreateRenderTarget(uint32_t width, uint32_t height)
    {
        m_SwapChainBuffers.resize(m_SwapChainDesc.BufferCount);
        for(uint32_t i = 0; i < GetBackBufferCount(); ++i){
            RefPtr<ID3D12Resource> resource;
            auto hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(resource.GetAddressOf()));
            DSM_CORE_ASSERT(SUCCEEDED(hr), "Get swapchain buffer failed.");
            
            m_SwapChainDesc.Width = width;
            m_SwapChainDesc.Height = height;
            TextureDesc texDesc{};
            texDesc.width = m_SwapChainDesc.Width;
            texDesc.height = m_SwapChainDesc.Height;
            texDesc.format = desc.swapChainFormat;
            texDesc.sampleCount = desc.swapChainSampleCount;
            texDesc.sampleQuality = desc.swapChainSampleQuality;
            texDesc.initialState = ResourceStates::Present;
            texDesc.isRenderTarget = true;
            texDesc.keepInitialState = true;
            texDesc.debugName = "SwapChain Buffer";

            m_SwapChainBuffers[i] = device->CreateHandleForNativeTexture(ObjectTypes::D3D12_Resource, resource.Get(), texDesc);
        }
    }
}
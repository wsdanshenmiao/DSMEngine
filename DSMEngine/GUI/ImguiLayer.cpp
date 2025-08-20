#include "ImguiLayer.h"
#include "imgui.h"
#include "Graphics/Device.h"
#include "Core/Application.h"
#include "backends/imgui_impl_glfw.h"

#if defined(DSM_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"
#include "Graphics/D3D12.h"

static DSM::D3D12::DescriptorHeapHandle s_DescriptorHeap = nullptr;
#endif

namespace DSM {
    ImguiLayer::ImguiLayer(IDevice *device, const Window& window)
        : Layer("ImguiLayer"), m_Device(device), m_Window(window) {}

    void ImguiLayer::OnAttach()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        io.MouseDrawCursor = true;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOther((GLFWwindow*)m_Window.GetNativeWindow(), true);

        // 根据使用的图形 API 及平台进行初始化
        switch (m_Device->GetGraphicsAPI()) {
#if defined(DSM_PLATFORM_WINDOWS)
        case GraphicsAPI::D3D12:{
            D3D12::IDevice* device = Utility::CheckedCast<D3D12::IDevice*>(m_Device);
            if(s_DescriptorHeap == nullptr){
                s_DescriptorHeap = device->CreateDescriptorHeap(D3D12::DescriptorHeapType::ShaderResourceView, 64, true);
            }
            ImGui_ImplDX12_InitInfo init_info{};
            init_info.Device = device->GetNativeObject(ObjectTypes::D3D12_Device);
            init_info.CommandQueue = device->GetNativeQueue(ObjectTypes::D3D12_CommandQueue, CommandQueueType::Graphics);
            init_info.NumFramesInFlight = 2;
            init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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
#endif
        default:
            break;
        }
    }

    void ImguiLayer::OnDetach()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImguiLayer::Begin(IFramebuffer* fb)
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
	}

    void ImguiLayer::End(IFramebuffer* fb)
    {
        assert(fb != nullptr);

		ImGui::Render();
        
#if defined(DSM_PLATFORM_WINDOWS)

        auto cmdList = m_Device->CreateCommandList(
            CommandListParameters().SetQueueType(CommandQueueType::Graphics));
        
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
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), nativeList);

        for(const auto& rt : fb->GetDesc().colorAttachments){
            if (rt.texture == nullptr) continue;
            cmdList->SetTextureState(rt.texture, rt.subresources, ResourceStates::Present);
        }
        cmdList->CommitBarriers();

        cmdList->Close();
        m_Device->ExecuteCommandList(cmdList);
#endif
    }
} // namespace DSM
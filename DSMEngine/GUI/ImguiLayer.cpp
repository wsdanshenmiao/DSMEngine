#include "ImguiLayer.h"
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"


namespace DSM {
    ImguiLayer::ImguiLayer(ID3D12Device *device, DXGI_FORMAT rtvFormat, HWND hwnd) 
        : Layer("ImguiLayer"), m_RtvFormat(rtvFormat), m_HWND(hwnd)
    {
        DSM_ASSERT(device != nullptr);
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NodeMask = 0;
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_SrvHeap.GetAddressOf()));
    }

    void ImguiLayer::OnAttach()
    {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// 允许键盘控制
        
		ImGui::StyleColorsDark();
        
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        m_SrvHeap->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

		ImGui_ImplWin32_Init(m_HWND);
		ImGui_ImplDX12_Init(
			device.Get(),
			1,
			m_RtvFormat,
			m_SrvHeap.Get(),
			m_SrvHeap->GetCPUDescriptorHandleForHeapStart(),
			m_SrvHeap->GetGPUDescriptorHandleForHeapStart());
    }

    void ImguiLayer::OnDetach()
    {
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplDX12_Shutdown();
        ImGui::DestroyContext();
    }
    
    void ImguiLayer::OnEvent(Event &event)
    {
    }
} // namespace DSM
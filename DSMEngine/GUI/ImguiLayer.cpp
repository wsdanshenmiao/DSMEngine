#include "ImguiLayer.h"
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"


namespace DSM {
    void ImguiLayer::OnAttach()
    {
        ImGui::CreateContext();
    }

    void ImguiLayer::OnDetach()
    {
        ImGui::DestroyContext();
    }

    void ImguiLayer::OnEvent(Event &event)
    {
        
    }

    void ImguiLayer::Begin()
    {
		ImGui::NewFrame();
	}

    void ImguiLayer::End()
    {
		ImGui::Render();
    }
} // namespace DSM
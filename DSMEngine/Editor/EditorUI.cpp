#include "EditorUI.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

namespace DSM {
    EditorUI::EditorUI(const EditorUIDesc& desc)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        io.MouseDrawCursor = true;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOther(desc.window->GetNativeWindow(), true);

        // 根据不同的图形 API 初始化不同的 ImGui 后端
        desc.renderer->InitWindowUI(this);
    }

    void EditorUI::Render()
    {
        static bool show = true;
        ImGui::ShowDemoWindow(&show);
    }
} // namespace DSM
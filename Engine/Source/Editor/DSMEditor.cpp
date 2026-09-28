#include "DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/Project.h"

#include <filesystem>
#include <imgui.h>

namespace DSM{
    void DSMEditor::StartEditor(DSMEngine *engine)
    {
        if(engine == nullptr){
            return;
        }
        m_Engine = engine;

        m_EditorUI = std::make_unique<EditorUI>(this);
    }

    bool DSMEditor::RunFrame()
    {
        if (m_Engine == nullptr || !m_Engine->IsRunning()) {
            return false;
        }

        if(m_ShouldResizeRenderer && m_ResizeWidth > 0 && m_ResizeHeight > 0) {
            if (auto renderer = DSMEngine::sm_GlobalContext.renderer) {
                renderer->ResizeRenderTexture(m_ResizeWidth, m_ResizeHeight);
            }
            m_ShouldResizeRenderer = false;
            m_ResizeWidth = m_ResizeHeight = 0;
        }

        m_Engine->Update();
        // ImGui 在渲染帧末处理事件，提供下一帧使用的语义化输入。
        if (auto input = DSMEngine::sm_GlobalContext.inputSystem;
            input != nullptr && ImGui::GetCurrentContext() != nullptr) {
            const auto& io = ImGui::GetIO();
            CameraInputState state{};
            state.forward = static_cast<int>(ImGui::IsKeyDown(ImGuiKey_W)) -
                static_cast<int>(ImGui::IsKeyDown(ImGuiKey_S));
            state.strafe = static_cast<int>(ImGui::IsKeyDown(ImGuiKey_D)) -
                static_cast<int>(ImGui::IsKeyDown(ImGuiKey_A));
            state.rotate = ImGui::IsMouseDragging(ImGuiMouseButton_Right);
            state.mouseDeltaX = io.MouseDelta.x;
            state.mouseDeltaY = io.MouseDelta.y;
            input->SetCameraInputOverride(state);
        }
        return m_Engine->IsRunning();
    }

    void DSMEditor::Run()
    {
        while (RunFrame()) {
        }
    }
    
    void DSMEditor::ShutDownEditor()
    {
        // 退出时先保存当前的项目
        Project::GetInstance().SaveProject(Project::GetInstance().GetFilePath());
        if (auto input = DSMEngine::sm_GlobalContext.inputSystem) {
            input->ClearCameraInputOverride();
        }
        m_EditorUI.reset();
        m_Engine = nullptr;
    }
}

#include "EditorUI.h"
#include "Editor/DSMEditor.h"
#include "Editor/SceneSerializer.h"
#include "Editor/AssertDefine.h"
#include "Editor/EditorUI/EditorViewport.h"
#include "Editor/EditorUI/EditorStyle.h"
#include "Editor/EditorUI/EditorConsole.h"
#include "Editor/EditorUI/EditorSceneHierarchy.h"
#include "Editor/EditorUI/EditorProperties.h"
#include "Editor/EditorUI/EditorContentBrowser.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Event/KeyEvent.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/Render/TextureManager.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <ImGuizmo.h>

namespace DSM {
    EditorUI::EditorUI(const EditorUIDesc& desc)
        :m_MenuBar(std::make_unique<EditorMenuBar>())
    {
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
        
        ImGui_ImplGlfw_InitForOther(desc.window->GetNativeWindow(), true);

        // 根据不同的图形 API 初始化不同的 ImGui 后端
        desc.renderer->InitWindowUI(this);

        OnSceneChange(DSMEngine::sm_GlobalContext.scene);

        m_PlayIcon = TextureManager::LoadTextureFromFile("Textures\\Icons\\PlayButton.png");
        m_StopIcon = TextureManager::LoadTextureFromFile("Textures\\Icons\\StopButton.png");
        DSM_CORE_ASSERT(m_PlayIcon != nullptr, "Failed to load play icon texture!");
        DSM_CORE_ASSERT(m_StopIcon != nullptr, "Failed to load pause icon texture!");

        m_Widgets.push_back(std::make_unique<EditorViewport>(this));
        m_Widgets.push_back(std::make_unique<EditorStyle>(this));
        m_Widgets.push_back(std::make_unique<EditorConsole>(this));
        m_Widgets.push_back(std::make_unique<EditorSceneHierarchy>(this));
        m_Widgets.push_back(std::make_unique<EditorProperties>(this));
        m_Widgets.push_back(std::make_unique<EditorContentBrowser>(this));
    }

    void EditorUI::OnGUI()
    {
        // Begin dockspace window

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | 
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoCollapse | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoBringToFrontOnFocus | 
            ImGuiWindowFlags_NoNavFocus;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        // Begin the dockspace window
        static bool dockspaceOpen = true;
        ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        }

        m_MenuBar->OnGUI();
        for(auto& widget : m_Widgets){
            widget->OnGUI();
        }
        m_ActiveScene->OnGUI();

        RenderUIToolbar();

        // End the dockspace window
        ImGui::End();
    }

    void EditorUI::OnEvent(Event &event)
    {
    }

    void EditorUI::OnSceneChange(std::shared_ptr<Scene> scene)
    {
        m_ActiveScene = scene;
    }

    void EditorUI::RenderUIToolbar()
    {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		auto& colors = ImGui::GetStyle().Colors;
		const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
		const auto& buttonActive = colors[ImGuiCol_ButtonActive];
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

        ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        bool enableToolbar = m_ActiveScene != nullptr;

		ImVec4 tintColor = ImVec4(1, 1, 1, 1);
    	if (!enableToolbar)
			tintColor.w = 0.5f;

		float size = ImGui::GetWindowHeight() - 4.0f;
		ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));

        bool hasPlayButton = m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play;
        bool hasPauseButton = m_SceneState != SceneState::Edit;

        if(hasPlayButton){
            auto iconTex = m_SceneState == SceneState::Edit ? m_PlayIcon : m_StopIcon;
            // TODO: 后续更换为通用的资源视图
            auto gpuHandle = iconTex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
            if(ImGui::ImageButton("##PlayButton", ImTextureRef{gpuHandle}, ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0,0,0,0), tintColor) && enableToolbar){
                if (m_SceneState == SceneState::Edit){
                    OnScenePlay();
                }
				else if (m_SceneState == SceneState::Play){
                    OnSceneStop();
                }
            };
        }

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(3);
		ImGui::End();
    }

    void EditorUI::OnScenePlay()
    {
        m_SceneState = SceneState::Play;
        auto tmpScene = std::make_shared<Scene>(*m_ActiveScene);
        OnSceneChange(tmpScene);
    }

    void EditorUI::OnSceneStop()
    {
        DSM_CORE_ASSERT(m_SceneState == SceneState::Play, "Scene is not in play state!");
        
        m_SceneState = SceneState::Edit;
        OnSceneChange(DSMEngine::sm_GlobalContext.scene);
    }

} // namespace DSM
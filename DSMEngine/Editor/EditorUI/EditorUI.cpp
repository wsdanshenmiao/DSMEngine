#include "EditorUI.h"
#include "Editor/DSMEditor.h"
#include "Editor/Project.h"
#include "Editor/EditorUI/EditorStyle.h"
#include "Editor/EditorUI/EditorConsole.h"
#include "Editor/EditorUI/EditorViewport.h"
#include "Editor/EditorUI/EditorProperties.h"
#include "Editor/EditorUI/EditorSceneHierarchy.h"
#include "Editor/EditorUI/EditorContentBrowser.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Event/KeyEvent.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/Platform/PlatformUtils.h"
#include "Runtime/Render/TextureManager.h"

#include <backends/imgui_impl_glfw.h>
#include <ImGuizmo.h>
#include <filesystem>

namespace DSM {
    EditorUI::EditorUI(DSMEditor* editor)
        :m_Editor(editor), m_MenuBar(std::make_unique<EditorMenuBar>(this))
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;
        io.ConfigWindowsResizeFromEdges  = true;
        io.FontDefault = m_Font = ImGui::GetIO().Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto.ttf", 12.f);
        
        ImGui_ImplGlfw_InitForOther(DSMEngine::sm_GlobalContext.window->GetNativeWindow(), true);

        // 根据不同的图形 API 初始化不同的 ImGui 后端
        DSMEngine::sm_GlobalContext.renderer->InitWindowUI(this);

        m_Widgets.push_back(std::make_unique<EditorViewport>(this));
        m_Widgets.push_back(std::make_unique<EditorStyle>(this));
        m_Widgets.push_back(std::make_unique<EditorConsole>(this));
        m_Widgets.push_back(std::make_unique<EditorSceneHierarchy>(this));
        m_Widgets.push_back(std::make_unique<EditorProperties>(this));
        m_Widgets.push_back(std::make_unique<EditorContentBrowser>(this));
    }

    EditorUI::~EditorUI()
    {
        DSMEngine::sm_GlobalContext.renderer->DestroyWindowUI();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    bool EditorUI::DrawProjectGateModal()
    {
        auto& project = Project::GetInstance();
        if(project.IsProjectOpen()) {
            return false;
        }

        auto projectFilters = std::vector<Utility::FileDialogs::FilterOption>{
            {"DSM Project Files", "*" + std::string(Project::s_ProjectFileExtension)}
        };

        auto applyProjectPathAndEnsureFolders = [](const std::filesystem::path& projectFilePath) {
            const std::filesystem::path projectDir = projectFilePath.parent_path();
            std::filesystem::create_directories(projectDir / Project::s_AssetsFolderName);
            std::filesystem::create_directories(projectDir / Project::s_LibraryFolderName);
        };

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::OpenPopup("##ProjectGateModal");
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 170.0f), ImGuiCond_Always);

        constexpr ImGuiWindowFlags modalFlags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;

        if(ImGui::BeginPopupModal("##ProjectGateModal", nullptr, modalFlags)) {
            ImGui::TextUnformatted("No project is loaded");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextUnformatted("Please create a new project or load an existing one");
            ImGui::Spacing();
            ImGui::Spacing();

            const float buttonWidth = 140.0f;
            const float totalButtonsWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
            const float startX = (ImGui::GetWindowSize().x - totalButtonsWidth) * 0.5f;
            ImGui::SetCursorPosX(startX > 0.0f ? startX : 0.0f);

            if(ImGui::Button("Create Project", ImVec2(buttonWidth, 0.0f))) {
                auto projPath = Utility::FileDialogs::SaveFile(projectFilters, "Create Project");
                if(!projPath.empty()) {
                    std::filesystem::path projectFilePath = projPath[0];
                    applyProjectPathAndEnsureFolders(projectFilePath);
                    project.SaveProject(projPath[0]);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if(ImGui::Button("Load Project", ImVec2(buttonWidth, 0.0f))) {
                auto projPath = Utility::FileDialogs::OpenFile(projectFilters, "Load Project");
                if(!projPath.empty()) {
                    applyProjectPathAndEnsureFolders(std::filesystem::path(projPath[0]));
                    project.LoadProject(projPath[0]);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }

        return !project.IsProjectOpen();
    }

    void EditorUI::OnGUI()
    {
        // Begin dockspace window

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags =
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
        ImGui::PopStyleVar(3);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::PopStyleVar();
        }

        if(DrawProjectGateModal()) {
            ImGui::End();
            return;
        }

        for(auto& widget : m_Widgets){
            widget->OnGUI();
        }
        DSMEngine::sm_GlobalContext.scene->OnGUI();
        m_MenuBar->OnGUI();

        // End the dockspace window
        ImGui::End();
    }

    void EditorUI::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e){
            auto inputSystem = DSMEngine::sm_GlobalContext.inputSystem;
            bool isCtrlPressed = inputSystem->IsKeyPressed(KeyCode::LeftControl) || 
                inputSystem->IsKeyPressed(KeyCode::RightControl);
            switch (e.GetKeyCode()) {
            case KeyCode::S:{
                break;
            }
            default:
                break;
            }
            return false;
        });

        for(auto& widget : m_Widgets){
            widget->OnEvent(event);
        }
    }

    void EditorUI::OnScenePlay()
    {
        m_InactiveScene = DSMEngine::sm_GlobalContext.scene;
        DSMEngine::sm_GlobalContext.scene = std::make_shared<Scene>(*m_InactiveScene);
    }

    void EditorUI::OnSceneStop()
    {
        DSMEngine::sm_GlobalContext.scene = m_InactiveScene;
        m_InactiveScene = nullptr;
    }


} // namespace DSM
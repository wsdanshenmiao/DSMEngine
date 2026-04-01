#include "EditorMenuBar.h"
#include "Editor/DSMEditor.h"
#include "Editor/SceneManager.h"
#include "Editor/AssertDefine.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/EditorStyle.h"
#include "Editor/EditorUI/EditorConsole.h"
#include "Editor/EditorUI/EditorViewport.h"
#include "Editor/EditorUI/EditorProperties.h"
#include "Editor/EditorUI/EditorContentBrowser.h"
#include "Editor/EditorUI/EditorSceneHierarchy.h"
#include "Runtime/Render/TextureManager.h"
#include <imgui.h>

namespace DSM {
    EditorMenuBar::EditorMenuBar(EditorUI* editorUI)
        :m_EditorUI(editorUI) 
    {
        m_PlayIcon = TextureManager::LoadTextureFromFile("Assets/Textures/Icons/PlayButton.png");
        m_StopIcon = TextureManager::LoadTextureFromFile("Assets/Textures/Icons/StopButton.png");
    }

    void EditorMenuBar::OnGUI()
    {
        auto& style = ImGui::GetStyle();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{style.FramePadding.x, 8});
        
        if(ImGui::BeginMainMenuBar()){
            ProjectMenuGUI();
            WorldMenuGUI();
            ViewMenuGUI();
            ButtonToolBar();

            ImGui::EndMainMenuBar();
        }

        ImGui::PopStyleVar();  
    }

    void EditorMenuBar::ProjectMenuGUI()
    {
        if(ImGui::BeginMenu("Project")){
            if(ImGui::MenuItem("New Project")){
                
            }
            ImGui::Separator();

            if(ImGui::MenuItem("Open Project")){
                
            }
            ImGui::Separator();

            if(ImGui::MenuItem("Save Project")){
                
            }

            ImGui::EndMenu();
        }
    }

    void EditorMenuBar::WorldMenuGUI()
    {
        if (ImGui::BeginMenu("Scene")){
            if(ImGui::MenuItem("New Scene")){
                SceneManager::NewScene();
            }
            ImGui::Separator();

            std::string fileExtension = g_SceneFileExtension;
            if(ImGui::MenuItem("Load Scene")){
                std::string filter = "DSM Scene Files (*" + fileExtension + ")\0*" + fileExtension + "\0";
                Utility::FileDialogs::FilterOption filterOption{"DSM Scene Files", "*" + fileExtension};
                auto filepath = Utility::FileDialogs::OpenFile({filterOption}, "Load Scene");
                if(!filepath.empty()){
                    SceneManager::LoadScene(filepath[0]);
                }
            }
            ImGui::Separator();

            if(ImGui::MenuItem("Save Scene")){
                std::string filter = "DSM Scene Files (*" + fileExtension + ")\0*" + fileExtension + "\0";
                Utility::FileDialogs::FilterOption filterOption{"DSM Scene Files", "*" + fileExtension};
                auto filepath = Utility::FileDialogs::SaveFile({filterOption}, "Save Scene");
                if(!filepath.empty()){
                    SceneManager::SaveScene(filepath[0]);
                }
            }

            ImGui::EndMenu();
        }
    }
    
    void EditorMenuBar::ViewMenuGUI()
    {
        if(ImGui::BeginMenu("View")){
            auto guiView = [this] <typename T>(){
                auto widget = m_EditorUI->GetWidget<T>();
                if(widget != nullptr && ImGui::MenuItem(widget->GetTitle(), nullptr, widget->IsEnabled())){
                    widget->SetEnabled(!widget->IsEnabled());
                }
            };

            guiView.operator()<EditorViewport>();
            guiView.operator()<EditorSceneHierarchy>();
            guiView.operator()<EditorProperties>();
            guiView.operator()<EditorConsole>();
            guiView.operator()<EditorContentBrowser>();
            guiView.operator()<DSM::EditorStyle>();
            ImGui::EndMenu();
        }
    }
    
    void EditorMenuBar::ButtonToolBar()
    {
        const float buttonSize = 16.0f;

        auto toolbarButton = [buttonSize] (
            ITexture* tex, 
            const char* text, 
            auto isEnabledFunc, 
            auto onPressFunc, 
            float cursorPosX = -1){
            ImGui::SameLine();
            auto buttonCol = isEnabledFunc() ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive] : ImGui::GetStyle().Colors[ImGuiCol_Button];
            ImGui::PushStyleColor(ImGuiCol_Button, buttonCol);

            if (cursorPosX > 0.0f) {
                ImGui::SetCursorPosX(cursorPosX);
            }

            const ImGuiStyle& style   = ImGui::GetStyle();
            const float size_avail_y  = 2.0f * style.FramePadding.y + buttonSize;
            const float button_size_y = buttonSize + 2.0f * GetPaddingY();
            const float offset_y      = (button_size_y - size_avail_y) * 0.5f;

            ImGui::SetCursorPosY(offset_y);

            // TODO: 后续更换为通用的资源视图
            auto texRef = ImTextureRef{ tex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor) };
            if (ImGui::ImageButton(text, texRef, ImVec2(buttonSize, buttonSize))) {
                onPressFunc();
            }

            ImGui::PopStyleColor();
        };
    
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float size_avail_x      = viewport->Size.x;
        const float button_size_final = buttonSize + GetPaddingX() * 2.0f;
        float num_buttons             = 1.0f;
        float size_toolbar            = num_buttons * button_size_final;
        float cursor_pos_x            = (size_avail_x - size_toolbar) * 0.5f;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 18.0f, GetPaddingY() - 5.0f });
        static auto isPlaying     = [this]() { return m_SceneState == SceneState::Play; };
        static auto togglePlaying = [this]() { m_SceneState = (m_SceneState == SceneState::Play) ? SceneState::Edit : SceneState::Play; };
        toolbarButton(
            isPlaying() ? m_StopIcon.Get() : m_PlayIcon.Get(),
            "Play",
            isPlaying,
            togglePlaying,
            cursor_pos_x
        );
        ImGui::PopStyleVar(1);
    }
}
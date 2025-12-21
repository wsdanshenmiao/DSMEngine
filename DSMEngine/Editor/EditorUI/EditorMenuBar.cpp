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
#include <imgui.h>

namespace DSM {
    EditorMenuBar::EditorMenuBar(EditorUI* editorUI)
        :m_EditorUI(editorUI) { }

    void EditorMenuBar::OnGUI()
    {
        auto& style = ImGui::GetStyle();
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{style.FramePadding.x, 8});
            
            if(ImGui::BeginMenuBar()){
                WorldMenuGUI();
                ViewMenuGUI();

                ImGui::EndMenuBar();
            }

            ImGui::PopStyleVar();   
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

            guiView.template operator()<EditorViewport>();
            guiView.template operator()<EditorSceneHierarchy>();
            guiView.template operator()<EditorProperties>();
            guiView.template operator()<EditorConsole>();
            guiView.template operator()<EditorContentBrowser>();
            guiView.template operator()<DSM::EditorStyle>();
            ImGui::EndMenu();
        }
    }
}
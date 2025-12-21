#include "EditorMenuBar.h"
#include "Editor/DSMEditor.h"
#include "Editor/SceneManager.h"
#include "Editor/AssertDefine.h"
#include <imgui.h>

namespace DSM {
    void EditorMenuBar::OnGUI()
    {
        auto& style = ImGui::GetStyle();
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{style.FramePadding.x, 8});
            
            if(ImGui::BeginMenuBar()){
                WorldMenuGUI();

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
}
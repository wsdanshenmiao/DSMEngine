#include "EditorMenuBar.h"
#include "Editor/DSMEditor.h"
#include "Editor/Project.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/EditorStyle.h"
#include "Editor/EditorUI/EditorConsole.h"
#include "Editor/EditorUI/EditorViewport.h"
#include "Editor/EditorUI/EditorProperties.h"
#include "Editor/EditorUI/EditorContentBrowser.h"
#include "Editor/EditorUI/EditorSceneHierarchy.h"
#include "Runtime/Render/TextureManager.h"
#include "Runtime/Platform/PlatformUtils.h"
#include "Runtime/Render/Renderer/DeferredRenderer/DeferredRenderPipeline.h"
#include "Runtime/Render/Renderer/ForwardRenderer/ForwardRenderPipeline.h"
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
            FileMenuGUI();
            WorldMenuGUI();
            ViewMenuGUI();
            RenderMenuGUI();
            ButtonToolBar();

            ImGui::EndMainMenuBar();
        }

        ImGui::PopStyleVar();  
    }

    void EditorMenuBar::FileMenuGUI()
    {
        if(ImGui::BeginMenu("File")){
            auto& project = Project::GetInstance();

            if(ImGui::MenuItem("New Scene")){
                project.NewScene();
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Open Scene")){
                auto scenePath = Utility::FileDialogs::OpenFile({{"DSM Scene Files", "*" + std::string(Project::s_SceneFileExtension)}}, "Open Scene");
                if (!scenePath.empty()) {
                    project.LoadScene(scenePath[0]);
                }
            }
            ImGui::Separator();


            if(ImGui::MenuItem("New Project")){
                project.NewProject();
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Open Project")){
                auto projPath = Utility::FileDialogs::OpenFile({{"DSM Project Files", "*" + std::string(Project::s_ProjectFileExtension)}}, "Open Project");
                if (!projPath.empty()) {
                    project.LoadProject(projPath[0]);
                }
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Save Project")){
                auto projPath = Utility::FileDialogs::SaveFile({{"DSM Project Files", "*" + std::string(Project::s_ProjectFileExtension)}}, "Save Project");
                if (!projPath.empty()) {
                    project.SaveProject(projPath[0]);
                }
            }

            if(ImGui::MenuItem("Exit")){
                if(project.GetFilePath().empty()){
                    auto projPath = Utility::FileDialogs::SaveFile({{"DSM Project Files", "*" + std::string(Project::s_ProjectFileExtension)}}, "Save Project");
                    if (!projPath.empty()) {
                        project.SaveProject(projPath[0]);
                    }
                }
                m_EditorUI->GetEditor()->GetEngine()->Close();
            }

            ImGui::EndMenu();
        }
    }

    void EditorMenuBar::WorldMenuGUI()
    {
        // if (ImGui::BeginMenu("Scene")){
        //     if(ImGui::MenuItem("New Scene")){
        //         ProjectManager::NewScene();
        //     }
        //     ImGui::Separator();

        //     std::string fileExtension = g_SceneFileExtension;
        //     if(ImGui::MenuItem("Load Scene")){
        //         std::string filter = "DSM Scene Files (*" + fileExtension + ")\0*" + fileExtension + "\0";
        //         Utility::FileDialogs::FilterOption filterOption{"DSM Scene Files", "*" + fileExtension};
        //         auto filepath = Utility::FileDialogs::OpenFile({filterOption}, "Load Scene");
        //         if(!filepath.empty()){
        //             ProjectManager::LoadScene(filepath[0]);
        //         }
        //     }
        //     ImGui::Separator();

        //     if(ImGui::MenuItem("Save Scene")){
        //         std::string filter = "DSM Scene Files (*" + fileExtension + ")\0*" + fileExtension + "\0";
        //         Utility::FileDialogs::FilterOption filterOption{"DSM Scene Files", "*" + fileExtension};
        //         auto filepath = Utility::FileDialogs::SaveFile({filterOption}, "Save Scene");
        //         if(!filepath.empty()){
        //             ProjectManager::SaveScene(filepath[0]);
        //         }
        //     }

        //     ImGui::EndMenu();
        // }
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
    
   void EditorMenuBar::RenderMenuGUI()
   {
       if (ImGui::BeginMenu("Render")) {
            auto engine = m_EditorUI->GetEditor()->GetEngine();
           auto& renderer = *DSMEngine::sm_GlobalContext.renderer;

           if (ImGui::MenuItem("Deferred", nullptr, m_IsDeferred)) {
               m_IsDeferred = true;
               renderer.GetDevice()->WaitForIdle();
               renderer.ResetRenderPipeline();
               engine->SetRenderPipeline(std::make_unique<DeferredRenderPipeline>());
           }
           if (ImGui::MenuItem("Forward", nullptr, !m_IsDeferred)) {
               m_IsDeferred = false;
               renderer.GetDevice()->WaitForIdle();
               renderer.ResetRenderPipeline();
               engine->SetRenderPipeline(std::make_unique<ForwardRenderPipeline>());
           }

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

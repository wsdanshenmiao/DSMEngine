#include "EditorSceneHierarchy.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/DSMEngine.h"
#include <numbers>

namespace DSM{
    EditorSceneHierarchy::EditorSceneHierarchy(EditorUI* editorUI)
        : Widget(editorUI)
    {
        m_Title = "Hierarchy";
        m_Flags |= ImGuiWindowFlags_HorizontalScrollbar;

        m_ComponentDrawerManager = std::make_unique< ComponentDrawerManager>();
    }

    void EditorSceneHierarchy::OnGUIEnabled()
    {
        bool isPlayMode = m_EditorUI->m_SceneState == EditorUI::SceneState::Play;
        ImGui::BeginDisabled(isPlayMode);

        ImGui::EndDisabled();

        auto scene = DSMEngine::sm_GlobalContext.scene;
        scene->TraverseAllEntity([this, scene](entt::entity entity) {
            std::shared_ptr<GameObject> object = scene->GetObjectByID(entity).lock();
            if (object != nullptr) {
                DrawEntityNode(object);
            }
        });

        if(ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()){
            m_SelectedObject.reset();
        }

        // 右键打开工具栏
        if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty GameObject"))
                scene->CreateObject("Empty GameObject");

            ImGui::EndPopup();
        }
    }

    // 绘制场景中实体的 UI 节点
    void EditorSceneHierarchy::DrawEntityNode(std::shared_ptr<GameObject> object)
    {
        if(object == nullptr)
            return;

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_AllowOverlap | 
            ImGuiTreeNodeFlags_SpanFullWidth | 
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_Leaf;
    
        const auto& name = object->GetComponent<TagComponent>()->tag;

        bool isSelected = m_SelectedObject.lock() == object;
        if(isSelected){
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }

        bool opened = ImGui::TreeNodeEx(object.get(), nodeFlags, name.c_str());
        // 当前节点被选中 
        if(ImGui::IsItemClicked()){
            m_SelectedObject = object;
        }

        bool entityDelete = false;
        if(ImGui::BeginPopupContextItem()){
            if(ImGui::MenuItem("Delete GameObject")){
                entityDelete = true;
            }
            ImGui::EndPopup();
        }

        if(opened){
            ImGui::TreePop();
        }

        if(entityDelete){
            DSMEngine::sm_GlobalContext.scene->DestroyObject(object->GetID());
            if(isSelected){
                m_SelectedObject.reset();
            }
        }
    }

    
}

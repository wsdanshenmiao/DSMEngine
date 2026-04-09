#include "EditorSceneHierarchy.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Event/KeyEvent.h"

#include <numbers>

namespace DSM{
    EditorSceneHierarchy::EditorSceneHierarchy(EditorUI* editorUI)
        : Widget(editorUI)
    {
        m_Title = "Hierarchy";
        m_Flags |= ImGuiWindowFlags_HorizontalScrollbar;
    }

    void EditorSceneHierarchy::OnGUIEnabled()
    {
        bool isPlayMode = m_EditorUI->GetMenuBar().GetSceneState() == EditorMenuBar::SceneState::Play;
        ImGui::BeginDisabled(isPlayMode);

        ImGui::EndDisabled();

        auto scene = DSMEngine::sm_GlobalContext.scene;
        for(const auto& rootObject : scene->GetRootObjects()){
            DrawEntityNode(rootObject);
        }

        if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()){
            m_SelectedObject.reset();
        }

        // 右键打开工具栏
        if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty GameObject"))
                scene->CreateObject("Empty GameObject");

            ImGui::EndPopup();
        }

        for(const auto& objID : m_ObjShouldDeleted){
            scene->DestroyObject(objID);
        }
    }

    void EditorSceneHierarchy::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e){
            switch(e.GetKeyCode()){
                case KeyCode::Delete:{
                    if(auto selectedObject = m_SelectedObject.lock()){
                        DSMEngine::sm_GlobalContext.scene->DestroyObject(selectedObject->GetID());
                        m_SelectedObject.reset();
                    }
                    break;
                }
                default:
                    break;
            }
            return false;
        });
    }

    // 绘制场景中实体的 UI 节点
    void EditorSceneHierarchy::DrawEntityNode(std::shared_ptr<GameObject> object)
    {
        if(object == nullptr)
            return;

        auto scene = DSMEngine::sm_GlobalContext.scene;
        const auto& children = object->GetChildren();
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth | 
            ImGuiTreeNodeFlags_OpenOnArrow;
        if(children.empty()){
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
    
        const auto& name = object->GetName();

        bool isSelected = m_SelectedObject.lock() == object;
        if(isSelected){
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::PushID(object.get());

        bool opened = ImGui::TreeNodeEx(object.get(), nodeFlags, name.c_str());
        // 当前节点被选中 
        if(ImGui::IsItemClicked()){
            m_SelectedObject = object;
        }

        // 右键打开工具栏
        bool entityDelete = false;
        ObjectID newChild = c_InvalidObjectID;
        if(ImGui::BeginPopupContextItem()){
            if(ImGui::MenuItem("Delete GameObject")){
                entityDelete = true;
            }
            if (ImGui::MenuItem("Create Empty GameObject")){
				newChild = scene->CreateObject("Empty GameObject");
            }

            ImGui::EndPopup();
        }

        if(!children.empty() && opened){
            for (const auto& child : children) {
                DrawEntityNode(child);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();

        if (entityDelete) {
            m_ObjShouldDeleted.push_back(object->GetID());
            if (isSelected) {
                m_SelectedObject.reset();
            }
        }

        if(newChild != c_InvalidObjectID){
            if(auto childObject = scene->GetObjectByID(newChild).lock()){
                childObject->SetParent(object);
            }
		}
    }

    
}

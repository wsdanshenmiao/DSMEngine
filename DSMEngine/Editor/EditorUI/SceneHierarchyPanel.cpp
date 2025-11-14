#include "SceneHierarchyPanel.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/DSMEngine.h"
#include <numbers>

namespace DSM{

    void SceneHierarchyPanel::SetScene(std::shared_ptr<Scene> scene)
    {
        m_Scene = scene;
        m_SelectedObject.reset();
    }
    
    void SceneHierarchyPanel::OnGUI()
    {
        // 场景中的所有物体
        ImGui::Begin("Scene Hierarchy");
        if(m_Scene != nullptr){
            m_Scene->TraverseAllEntity([this](entt::entity entity) {
                std::shared_ptr<GameObject> object = m_Scene->GetObjectByID(entity).lock();
                if (object != nullptr) {
                    DrawEntityNode(object);
                }
            });
        }

        if(ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()){
            m_SelectedObject.reset();
        }

        // 右键打开工具栏
        if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty GameObject"))
                DSMEngine::sm_GlobalContext.scene->CreateObject("Empty GameObject");

            ImGui::EndPopup();
        }

        ImGui::End();

        // 属性面板
        ImGui::Begin("Properties");
        if (auto selectedObject = GetSelectedObject().lock(); selectedObject != nullptr) {
            // 显示选中物体的属性
            m_ComponentDrawerManager->DrawComponentsUI(selectedObject);
        }
        ImGui::End();
    }

    // 绘制场景中实体的 UI 节点
    void SceneHierarchyPanel::DrawEntityNode(std::shared_ptr<GameObject> object)
    {
        const auto& name = object->GetComponent<TagComponent>()->tag;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        bool isSelected = m_SelectedObject.lock() == object;
        if(isSelected){
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool opened = ImGui::TreeNodeEx(object.get(), flags, name.c_str());
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

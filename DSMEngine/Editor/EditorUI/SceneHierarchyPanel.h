#pragma once
#ifndef __SCENEHIERARCHYPANEL_H__
#define __SCENEHIERARCHYPANEL_H__

#include <memory>
#include <imgui.h>
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Math/MathCommon.h"

namespace DSM {
    class Scene;
    class GameObject;

    // 场景的层级面板 UI
    class SceneHierarchyPanel
    {
    public:
        void SetScene(std::shared_ptr<Scene> scene);

        void OnGUI();

        std::weak_ptr<GameObject> GetSelectedObject() const { return m_SelectedObject; }
        void SetSelectedObject(std::weak_ptr<GameObject> object) { m_SelectedObject = object; }

    private:
        void DrawEntityNode(std::shared_ptr<GameObject> object);
        void DrawComponents(std::shared_ptr<GameObject> object);

        template<typename T, typename UIFunc>
        void DrawComponent(const std::string& name, std::shared_ptr<GameObject> object, UIFunc func);

        static void DrawVec3Control(const std::string& label, Math::Vector3& values, float resetValue = 0.0f, float columnWidth = 100.0f);

    private:
        std::shared_ptr<Scene> m_Scene;
        std::weak_ptr<GameObject> m_SelectedObject;
    };
    
    
    template <typename T, typename UIFunc>
    inline void SceneHierarchyPanel::DrawComponent(
        const std::string& name, 
        std::shared_ptr<GameObject> object, 
        UIFunc func)
    {
        if(!object->HasComponent<T>())
            return;

        const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
		
        ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4,4});
        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImGui::Separator();
        bool opened = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
        ImGui::PopStyleVar();
    
        if(opened){
            func(*object->GetComponent<T>());
            ImGui::TreePop();
        }
    }
} // namespace DSM

#endif  // __SCENEHIERARCHYPANEL_H__
#include "EditorProperties.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/EditorSceneHierarchy.h"


namespace DSM {
    EditorProperties::EditorProperties(EditorUI* editorUI)
        : Widget(editorUI),
        m_ComponentDrawerManager(std::make_unique<ComponentDrawerManager>())
    {
        m_Title = "Properties";
        m_Size.Set(0, 500);
    }

    void EditorProperties::OnGUIEnabled()
    {
        bool isPlayMode = m_EditorUI->GetMenuBar().GetSceneState() == EditorMenuBar::SceneState::Play;
        ImGui::BeginDisabled(isPlayMode);
        if (auto selectedObject = m_EditorUI->GetWidget<EditorSceneHierarchy>()->GetSelectedObject().lock(); 
            selectedObject != nullptr) {
            // 显示选中物体的属性
            m_ComponentDrawerManager->DrawComponentsUI(selectedObject);
        }
        ImGui::EndDisabled();
    }
}
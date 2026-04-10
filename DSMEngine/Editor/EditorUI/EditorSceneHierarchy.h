#pragma once
#ifndef __EDITORSCENEHIERARCHY_h__
#define __EDITORSCENEHIERARCHY_h__

#include <memory>
#include <vector>

#include "Editor/EditorUI/Widget.h"
#include "Runtime/Framework/Scene.h"

namespace DSM {
    class Scene;
    class GameObject;

    // 场景的层级面板 UI
    class EditorSceneHierarchy : public Widget
    {
    public:
        EditorSceneHierarchy(EditorUI* editorUI);
        ~EditorSceneHierarchy() noexcept override = default;

        void OnGUIEnabled() override;
        void OnEvent(Event& event) override;

        std::weak_ptr<GameObject> GetSelectedObject() const { return m_SelectedObject; }
        void SetSelectedObject(std::weak_ptr<GameObject> object) { m_SelectedObject = object; }

    private:
        void DrawEntityNode(std::shared_ptr<GameObject> object);

    private:
        std::weak_ptr<GameObject> m_SelectedObject;
        std::vector<ObjectID> m_ObjShouldDeleted{};
    };
    
} // namespace DSM

#endif  // __EDITORSCENEHIERARCHY_h__
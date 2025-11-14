#pragma once
#ifndef __SCENEHIERARCHYPANEL_H__
#define __SCENEHIERARCHYPANEL_H__

#include <memory>
#include "Editor/EditorUI/ComponentDrawer.h"

namespace DSM {
    class Scene;
    class GameObject;

    // 场景的层级面板 UI
    class SceneHierarchyPanel
    {
    public:
        SceneHierarchyPanel() : m_ComponentDrawerManager(std::make_unique<ComponentDrawerManager>()) {}
    
        void SetScene(std::shared_ptr<Scene> scene);

        void OnGUI();

        std::weak_ptr<GameObject> GetSelectedObject() const { return m_SelectedObject; }
        void SetSelectedObject(std::weak_ptr<GameObject> object) { m_SelectedObject = object; }

    private:
        void DrawEntityNode(std::shared_ptr<GameObject> object);

        template<typename T, typename UIFunc>
        void DrawComponent(const std::string& name, std::shared_ptr<GameObject> object, UIFunc func);

    private:
        std::shared_ptr<Scene> m_Scene;
        std::unique_ptr<ComponentDrawerManager> m_ComponentDrawerManager;
        std::weak_ptr<GameObject> m_SelectedObject;
    };
    
} // namespace DSM

#endif  // __SCENEHIERARCHYPANEL_H__
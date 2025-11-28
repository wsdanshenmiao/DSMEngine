#pragma once
#ifndef __EDITORUI_H__
#define __EDITORUI_H__

#include <memory>
#include "Runtime/Render/WindowUI.h"
#include "Editor/EditorUI/SceneHierarchyPanel.h"

namespace DSM {
    class Window;
    class Renderer;

    struct EditorUIDesc
    {
        std::shared_ptr<Renderer> renderer;
        std::shared_ptr<Window> window;
    };

    class EditorUI : public WindowUI
    {
    public:
        EditorUI(const EditorUIDesc& desc);

        void Render() override;
        void OnEvent(Event& event) override;

    private:
        void RenderViewportWindow();
        void RenderGizmo();

        void NewScene();
        void SaveScene();
        void LoadScene(const std::filesystem::path& filepath);

    private:
        std::unique_ptr<SceneHierarchyPanel> m_SceneHierarchyPanel;
        Math::Vector4 m_ViewportBounds;

        int m_GizmoType = -1;
    };
    
} // namespace DSM 

#endif
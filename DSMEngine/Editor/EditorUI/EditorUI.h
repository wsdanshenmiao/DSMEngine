#pragma once
#ifndef __EDITORUI_H__
#define __EDITORUI_H__

#include <memory>
#include "Runtime/Render/WindowUI.h"
#include "Editor/EditorUI/SceneHierarchyPanel.h"
#include "Editor/EditorUI/ContentBrowserPanel.h"

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
        void OnSceneChange(std::shared_ptr<Scene> scene);

        void RenderViewportWindow();
        void RenderGizmo();
        void RenderUIToolbar();

        void NewScene();
        void SaveScene();
        void LoadScene(const std::filesystem::path& filepath);

        void OnScenePlay();
        void OnSceneStop();

    private:
        enum class SceneState
        {
            Edit,
            Play,
            Pause
        };

        std::unique_ptr<SceneHierarchyPanel> m_SceneHierarchyPanel;
        std::unique_ptr<ContentBrowserPanel> m_ContentBrowserPanel;
        Math::Vector4 m_ViewportBounds;

        int m_GizmoType = -1;
        SceneState m_SceneState = SceneState::Edit;
        std::shared_ptr<Scene> m_ActiveScene;

        TextureHandle m_PlayIcon;
        TextureHandle m_PauseIcon;
    };
    
} // namespace DSM 

#endif
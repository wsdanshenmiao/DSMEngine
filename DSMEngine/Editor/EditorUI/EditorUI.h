#pragma once
#ifndef __EDITORUI_H__
#define __EDITORUI_H__

#include <memory>

#include "Runtime/Render/WindowUI.h"
#include "Runtime/Graphics/Texture.h"
#include "Editor/EditorUI/Widget.h"

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
        friend class DSMEditor;
    public:
        EditorUI(const EditorUIDesc& desc);

        void OnGUI() override;
        void OnEvent(Event& event) override;

        template <typename T>
        T* GetWidget()
        {
            for (const auto& widget : m_Widgets) {
                if (T* castedWidget = dynamic_cast<T*>(widget.get())) {
                    return castedWidget;
                }
            }
            return nullptr;
        }

    private:
        void OnSceneChange(std::shared_ptr<Scene> scene);

        void RenderGizmo();
        void RenderUIToolbar();

        void OnScenePlay();
        void OnSceneStop();

    public:
        enum class SceneState
        {
            Edit,
            Play,
            Pause
        };

        SceneState m_SceneState = SceneState::Edit;

    public:
        std::vector<std::unique_ptr<Widget>> m_Widgets;
        
        Math::Vector4 m_ViewportBounds;

        int m_GizmoType = -1;
        std::shared_ptr<Scene> m_ActiveScene;

        TextureHandle m_PlayIcon;
        TextureHandle m_StopIcon;
    };
    
} // namespace DSM 

#endif
#pragma once
#ifndef __EDITORUI_H__
#define __EDITORUI_H__

#include <memory>

#include "Runtime/Render/WindowUI.h"
#include "Runtime/Graphics/Texture.h"
#include "Editor/EditorUI/Widget.h"
#include "Editor/EditorUI/EditorMenuBar.h"

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
        EditorUI(DSMEditor* editor);

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
        DSMEditor* m_Editor;

        std::vector<std::unique_ptr<Widget>> m_Widgets;
        std::unique_ptr<EditorMenuBar> m_MenuBar;

        std::shared_ptr<Scene> m_InactiveScene;

        TextureHandle m_PlayIcon;
        TextureHandle m_StopIcon;
    };
    
} // namespace DSM 

#endif
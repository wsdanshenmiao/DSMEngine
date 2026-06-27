#pragma once
#ifndef __EDITORMENUBAR_H__
#define __EDITORMENUBAR_H__

#include "Runtime/Graphics/Texture.h"

namespace DSM {
    class EditorUI;

    class EditorMenuBar
    {
    public:
        enum class SceneState
        {
            Edit,
            Play,
            Pause
        };

        EditorMenuBar(EditorUI* editorUI);
        void OnGUI();

        SceneState GetSceneState() const { return m_SceneState; }

    private:
        void FileMenuGUI();
        void WorldMenuGUI();
        void ViewMenuGUI();
        void RenderMenuGUI();
        void ButtonToolBar();

        static float GetPaddingX() { return 14.0f; }
        static float GetPaddingY() { return 8.0f; }

    private:
        EditorUI* m_EditorUI;
        SceneState m_SceneState = SceneState::Edit;
        bool m_IsDeferred = true;

        TextureHandle m_PlayIcon;
        TextureHandle m_StopIcon;
    };
}

#endif

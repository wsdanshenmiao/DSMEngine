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

    private:
        std::unique_ptr<SceneHierarchyPanel> m_SceneHierarchyPanel;
    };
    
} // namespace DSM 

#endif
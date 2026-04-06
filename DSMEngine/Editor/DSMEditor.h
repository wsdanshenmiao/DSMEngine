#pragma once
#ifndef _DSM_EDITOR_H_
#define _DSM_EDITOR_H_

#include <memory>
#include <string>
#include "EditorUI/EditorUI.h"

namespace DSM {
    class DSMEngine;
    class Window;
    class GraphicsRenderer;
    class Widget;

    class DSMEditor
    {
    public:
        void StartEditor(DSMEngine* engine);
        void Run();
        void ShutDownEditor();

        DSMEngine* GetEngine() const { return m_Engine; }
        
        void SetShouldResizeRenderer(size_t width, size_t height) {
            m_ShouldResizeRenderer = true;
            m_ResizeWidth = std::max(width, 1zu);
            m_ResizeHeight = std::max(height, 1zu);
        }

    private:
        std::unique_ptr<EditorUI> m_EditorUI;
        DSMEngine* m_Engine;
        bool m_ShouldResizeRenderer = false;
        size_t m_ResizeWidth = 0;
        size_t m_ResizeHeight = 0;
    };
} // namespace DSM

#endif
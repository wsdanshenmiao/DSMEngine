#pragma once
#ifndef _DSM_EDITOR_H_
#define _DSM_EDITOR_H_

#include <memory>
#include "EditorUI/EditorUI.h"

namespace DSM {
    class DSMEngine;
    class Window;
    class Renderer;
    class Widget;

    class DSMEditor
    {
    public:
        void StartEditor(DSMEngine* engine);
        void Run();
        void ShutDownEditor();

        DSMEngine* GetEngine() const { return m_Engine; }

    private:
        std::unique_ptr<EditorUI> m_EditorUI;
        DSMEngine* m_Engine;
    };
} // namespace DSM

#endif
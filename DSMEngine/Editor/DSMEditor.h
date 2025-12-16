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

    struct EditorContext
    {
        DSMEngine* engine{};
        std::shared_ptr<Window> window{};
        std::shared_ptr<Renderer> renderer{};
    };

    class DSMEditor
    {
    public:
        void StartEditor(DSMEngine* engine);
        void Run();
        void ShutDownEditor();

    public:
        inline static EditorContext sm_EditorContext{};

    private:
        std::unique_ptr<EditorUI> m_EditorUI;
        DSMEngine* m_Engine;
    };
} // namespace DSM

#endif
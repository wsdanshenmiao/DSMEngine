#pragma once
#ifndef _DSM_EDITOR_H_
#define _DSM_EDITOR_H_

#include <memory>

namespace DSM {
    class DSMEngine;
    class EditorUI;
    class Window;
    class Renderer;

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
        std::shared_ptr<EditorUI> m_EditorUI;
        DSMEngine* m_Engine;
    };
} // namespace DSM

#endif
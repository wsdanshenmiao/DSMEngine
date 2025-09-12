#pragma once
#ifndef __EDITOR_CONTEXT_H__
#define __EDITOR_CONTEXT_H__

#include <memory>

namespace DSM{
    class DSMEngine;
    class Window;
    class Renderer;

    struct EditorContextDesc
    {
        DSMEngine* engine{};
        std::shared_ptr<Window> window;
        std::shared_ptr<Renderer> renderer;
    };

    struct EditorContext
    {
        DSMEngine* engine{};
        std::shared_ptr<Window> window{};
        std::shared_ptr<Renderer> renderer{};

        void CreateContext(const EditorContextDesc& desc);
        void ShutdownContext();
    };
    extern EditorContext g_EditorContext;
}


#endif // __EDITOR_CONTEXT_H__
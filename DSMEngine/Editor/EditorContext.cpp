#include "EditorContext.h"
#include <cassert>

namespace DSM {
    EditorContext g_EditorContext;

    void EditorContext::CreateContext(const EditorContextDesc& desc)
    {
        assert(desc.engine != nullptr && desc.renderer != nullptr && desc.window != nullptr);
        engine = desc.engine;
        window = desc.window;
        renderer = desc.renderer;
    }

    void EditorContext::ShutdownContext()
    {
        engine = nullptr;
        window.reset();
        renderer.reset();
    }
}
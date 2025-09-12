#pragma once
#ifndef _DSM_EDITOR_H_
#define _DSM_EDITOR_H_

#include <memory>

namespace DSM {
    class DSMEngine;
    class EditorUI;

    class DSMEditor
    {
    public:
        DSMEditor(DSMEngine* engine);
        ~DSMEditor();

        void Run();

    private:
        std::shared_ptr<EditorUI> m_EditorUI;
        DSMEngine* m_Engine;
    };
} // namespace DSM

#endif
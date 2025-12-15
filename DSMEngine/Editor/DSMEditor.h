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
        std::vector<std::shared_ptr<Widget>> widgets{};
    };

    class DSMEditor
    {
    public:
        void StartEditor(DSMEngine* engine);
        void Run();
        void ShutDownEditor();

        template <typename T>
        static T* GetWidget()
        {
            for (const auto& widget : sm_EditorContext.widgets) {
                if (T* castedWidget = dynamic_cast<T*>(widget.get())) {
                    return castedWidget;
                }
            }
            return nullptr;
        }

    public:
        inline static EditorContext sm_EditorContext{};

    private:
        std::unique_ptr<EditorUI> m_EditorUI;
        DSMEngine* m_Engine;
    };
} // namespace DSM

#endif
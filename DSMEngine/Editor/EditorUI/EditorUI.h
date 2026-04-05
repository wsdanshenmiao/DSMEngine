#pragma once
#ifndef __EDITORUI_H__
#define __EDITORUI_H__

#include <memory>

#include <imgui.h>

#include "Runtime/Render/WindowUI.h"
#include "Runtime/Graphics/Texture.h"
#include "Editor/EditorUI/Widget.h"
#include "Editor/EditorUI/EditorMenuBar.h"

namespace DSM {
    class Window;
    class GraphicsRenderer;

    struct EditorUIDesc
    {
        std::shared_ptr<GraphicsRenderer> renderer;
        std::shared_ptr<Window> window;
    };

    class EditorUI : public WindowUI
    {
        friend class DSMEditor;
    public:
        EditorUI(DSMEditor* editor);
        ~EditorUI() override;

        void OnGUI() override;
        void OnEvent(Event& event) override;

        const EditorMenuBar& GetMenuBar() const { return *m_MenuBar; }

        DSMEditor* GetEditor() const { return m_Editor; }

        template <typename T>
        T* GetWidget()
        {
            for (const auto& widget : m_Widgets) {
                if (T* castedWidget = dynamic_cast<T*>(widget.get())) {
                    return castedWidget;
                }
            }
            return nullptr;
        }

    private:
        bool DrawProjectGateModal();
        void OnScenePlay();
        void OnSceneStop();

    private:
        DSMEditor* m_Editor;

        std::vector<std::unique_ptr<Widget>> m_Widgets;
        std::unique_ptr<EditorMenuBar> m_MenuBar;

        std::shared_ptr<Scene> m_InactiveScene;

        ImFont* m_Font;
    };
    
} // namespace DSM 

#endif
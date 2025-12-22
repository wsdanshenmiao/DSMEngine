#include "DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Core/Window.h"
#include "Editor/EditorUI/EditorUI.h"

namespace DSM{
    void DSMEditor::StartEditor(DSMEngine *engine)
    {
        DSM_ASSERT(engine != nullptr, "Engine is nullptr!");
        m_Engine = engine;

        m_EditorUI = std::make_unique<EditorUI>(this);
    }

    void DSMEditor::Run()
    {
        while (m_Engine->IsRunning()) {
            m_Engine->Run();
        }
    }
    
    void DSMEditor::ShutDownEditor()
    {
        m_EditorUI.reset();
        m_Engine = nullptr;
    }
}
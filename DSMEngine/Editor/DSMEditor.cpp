#include "DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "EditorUI.h"
#include "EditorContext.h"

namespace DSM{
    DSMEditor::DSMEditor(DSMEngine* engine)
        : m_Engine(engine)
    {
        DSM_ASSERT(engine != nullptr, "Engine is nullptr!");

        g_EditorContext.CreateContext(EditorContextDesc{ 
            engine, g_GlobalContext.window, g_GlobalContext.renderer });
        m_EditorUI = std::make_shared<EditorUI>(EditorUIDesc{ 
            g_GlobalContext.renderer, g_GlobalContext.window });
    }

    DSMEditor::~DSMEditor()
    {
        m_EditorUI.reset();
        g_EditorContext.ShutdownContext();
        m_Engine = nullptr;
    }

    void DSMEditor::Run()
    {
        while (m_Engine->IsRunning()) {
            m_Engine->Run();
        }
        
    }
}
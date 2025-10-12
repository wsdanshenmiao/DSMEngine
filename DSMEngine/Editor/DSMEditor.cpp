#include "DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Editor/EditorUI/EditorUI.h"

namespace DSM{
    DSMEditor::DSMEditor(DSMEngine* engine)
        : m_Engine(engine)
    {
        DSM_ASSERT(engine != nullptr, "Engine is nullptr!");

        sm_EditorContext.engine = engine;
        sm_EditorContext.window = DSMEngine::sm_GlobalContext.window;
        sm_EditorContext.renderer = DSMEngine::sm_GlobalContext.renderer;
        
        m_EditorUI = std::make_shared<EditorUI>(EditorUIDesc{ 
            DSMEngine::sm_GlobalContext.renderer, DSMEngine::sm_GlobalContext.window });
    }

    DSMEditor::~DSMEditor()
    {
        m_EditorUI.reset();
        sm_EditorContext = {};
        m_Engine = nullptr;
    }

    void DSMEditor::Run()
    {
        while (m_Engine->IsRunning()) {
            m_Engine->Run();
        }
        
    }
}
#include "DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Core/Window.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/EditorViewport.h"
#include "Editor/EditorUI/EditorStyle.h"
#include "Editor/EditorUI/EditorConsole.h"

namespace DSM{
    void DSMEditor::StartEditor(DSMEngine *engine)
    {
        DSM_ASSERT(engine != nullptr, "Engine is nullptr!");
        m_Engine = engine;

        sm_EditorContext.engine = engine;
        sm_EditorContext.window = DSMEngine::sm_GlobalContext.window;
        sm_EditorContext.renderer = DSMEngine::sm_GlobalContext.renderer;

        m_EditorUI = std::make_unique<EditorUI>(EditorUIDesc{ 
            DSMEngine::sm_GlobalContext.renderer, 
            DSMEngine::sm_GlobalContext.window });

        
        auto& widgets = sm_EditorContext.widgets;
        widgets.clear();
        widgets.push_back(std::make_shared<EditorViewport>());
        widgets.push_back(std::make_shared<EditorStyle>());
        widgets.push_back(std::make_shared<EditorConsole>());
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
        sm_EditorContext = {};
        m_Engine = nullptr;
    }
}
#include "DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Core/Window.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/Project.h"

#include <filesystem>

namespace DSM{
    void DSMEditor::StartEditor(DSMEngine *engine)
    {
        if(engine == nullptr){
            return;
        }
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
        // 退出时先保存当前的项目
        Project::GetInstance().SaveProject(Project::GetInstance().GetFilePath());
        m_EditorUI.reset();
        m_Engine = nullptr;
    }
}
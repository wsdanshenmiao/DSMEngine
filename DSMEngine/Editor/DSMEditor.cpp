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

    bool DSMEditor::RunFrame()
    {
        if (m_Engine == nullptr || !m_Engine->IsRunning()) {
            return false;
        }

        if(m_ShouldResizeRenderer && m_ResizeWidth > 0 && m_ResizeHeight > 0) {
            if (auto renderer = DSMEngine::sm_GlobalContext.renderer) {
                renderer->ResizeRenderTexture(m_ResizeWidth, m_ResizeHeight);
            }
            m_ShouldResizeRenderer = false;
            m_ResizeWidth = m_ResizeHeight = 0;
        }

		m_Engine->Update();
        return m_Engine->IsRunning();
    }

    void DSMEditor::Run()
    {
        while (RunFrame()) {
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

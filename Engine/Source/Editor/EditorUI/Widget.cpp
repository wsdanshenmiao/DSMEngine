#include "Widget.h"
#include "Runtime/Core/Window.h"
#include "Editor/DSMEditor.h"
#include "Editor/EditorUI/EditorViewport.h"

#include <imgui_internal.h>

namespace DSM {
    Widget::Widget(EditorUI* editorUI)
        : m_EditorUI(editorUI),
        m_Flags(ImGuiWindowFlags_NoCollapse) {}

    void Widget::OnGUI()
    {
        OnGUIDefault();

        if(!m_Enabled)
            return;
        
        int varPushCount = 0;

        float width = DSMEngine::sm_GlobalContext.window->GetWidth();
        float height = DSMEngine::sm_GlobalContext.window->GetHeight();
        m_Size = m_Size == c_DefaultWidgetValue ? Math::Vector2{width, height} : m_Size;
    
        if(m_MinSize != c_DefaultWidgetValue || m_MaxSize != std::numeric_limits<float>::max()){
            ImGui::SetNextWindowSizeConstraints(
                ImVec2{m_MinSize.Get(0), m_MinSize.Get(1)},
                ImVec2{m_MaxSize.Get(0), m_MaxSize.Get(1)});
        }

        if(m_Padding != c_DefaultWidgetValue){
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{m_Padding.Get(0), m_Padding.Get(1)});
            varPushCount++;
        }

        if(m_Alpha != c_DefaultWidgetValue){
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Alpha);
            varPushCount++;
        }

        if (EditorViewport* viewport = m_EditorUI->GetWidget<EditorViewport>()) {
            if (ImGuiWindow* window = viewport->GetWindow()) {
                ImVec2 pos    = window->Pos;
                ImVec2 sze    = window->Size;
                ImVec2 center = ImVec2(pos.x + sze.x * 0.5f, pos.y + sze.y * 0.5f);
                ImVec2 pivot  = ImVec2(0.5f, 0.5f);

                ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, pivot);
            }
        }
    
        if(ImGui::Begin(m_Title, &m_Enabled, m_Flags) && GetWindow() && GetWindow()->Appearing){
            OnEnable();
        }
        else if(!m_Enabled){
            OnDisable();
        }

        OnGUIEnabled();

        ImGui::End();
        ImGui::PopStyleVar(varPushCount);
    }
    
    ImGuiWindow *Widget::GetWindow() const
    {
        return ImGui::GetCurrentWindow();
    }
}
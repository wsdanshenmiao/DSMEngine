#include "EditorViewport.h"
#include "Editor/DSMEditor.h"
#include "Runtime/Render/Renderer/Renderer.h"

#include <imgui.h>

namespace DSM {
    EditorViewport::EditorViewport()
        : Widget()
    {
        m_Title = "Viewport";
        m_Size = Math::Vector2{400.0f, 300.0f};
        m_Flags |= ImGuiWindowFlags_NoScrollbar;
        m_Padding = Math::Vector2{2, 2};
    }

    void EditorViewport::OnGUIEnabled()
    {
        // 调整渲染器的渲染目标大小以适应视口大小
        float width = ImGui::GetContentRegionAvail().x;
        float height = ImGui::GetContentRegionAvail().y;
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        auto& renderer = DSMEditor::sm_EditorContext.renderer;
        Viewport cameraViewport = renderer->GetCamera().GetViewPort();
        if(cameraViewport.Width() != width || cameraViewport.Height() != height){
            renderer->ResizeRenderTexture(width, height);
        }

        // 设置渲染图像
        auto colorTex = renderer->GetColorTexture();
        // TODO: 后续更换为跨平台接口
        auto gpuHandle = colorTex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
        ImGui::Image(ImTextureRef{gpuHandle}, viewportSize);

        if(ImGui::BeginDragDropTarget()){
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ContentBrowserPanel::sm_DragDropPayloadType)){
                const char* path = static_cast<const char*>(payload->Data);
                // LoadScene(path);
            }
            ImGui::EndDragDropTarget();
        }
    }
}
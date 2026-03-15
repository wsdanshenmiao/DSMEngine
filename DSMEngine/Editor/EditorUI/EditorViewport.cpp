#include "EditorViewport.h"
#include "Editor/DSMEditor.h"
#include "Editor/SceneManager.h"
#include "Editor/AssertDefine.h"
#include "Editor/EditorUI/EditorSceneHierarchy.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Event/KeyEvent.h"
#include "Runtime/Core/Input/InputSystem.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace DSM {
    EditorViewport::EditorViewport(EditorUI* editorUI)
        : Widget(editorUI)
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
        auto& renderer = DSMEngine::sm_GlobalContext.renderer;
        Viewport cameraViewport = renderer->GetCamera().GetViewPort();
        if(cameraViewport.Width() != width || cameraViewport.Height() != height){
            renderer->ResizeRenderTexture(std::max(width, 1.f), std::max(height, 1.f));
        }

        // 设置渲染图像
        auto colorTex = renderer->GetColorTexture();
        // TODO: 后续更换为跨平台接口
        auto gpuHandle = colorTex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
        ImGui::Image(ImTextureRef{gpuHandle}, viewportSize);

        if(ImGui::BeginDragDropTarget()){
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(g_ContentBrowserDragDropPayload)){
                const char* path = static_cast<const char*>(payload->Data);
                SceneManager::LoadScene(path);
            }
            ImGui::EndDragDropTarget();
        }
        
        auto selectedObject = m_EditorUI->GetWidget<EditorSceneHierarchy>()->GetSelectedObject().lock();
        if(selectedObject == nullptr || m_GizmoType == -1){
            return;
        }

        const auto& camera = DSMEngine::sm_GlobalContext.renderer->GetCamera();

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

        auto cameraView = camera.GetViewMatrix();
        auto cameraProj = camera.GetProjMatrix();
        auto transfrom = selectedObject->GetComponent<Math::Transform>();
        Math::Matrix4 transMat = transfrom->GetLocalToWorld();
        ImGuizmo::Manipulate((float*)&cameraView, (float*)&cameraProj, 
            static_cast<ImGuizmo::OPERATION>(m_GizmoType), ImGuizmo::LOCAL, (float*)&transMat);
        if(ImGuizmo::IsUsing()){
            *transfrom = Math::Transform{transMat};
        }
    }
    
    void EditorViewport::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
            if(e.IsRepeat()){
                return false;
            }
            
            switch (e.GetKeyCode()) {
            case KeyCode::R:{
                if(!ImGuizmo::IsUsing()){
                    m_GizmoType = ImGuizmo::ROTATE;
                }
                break;
            }
            case KeyCode::T:{
                if(!ImGuizmo::IsUsing()){
                    m_GizmoType = ImGuizmo::TRANSLATE;
                }
                break;
            }
            case KeyCode::Y:{
                if(!ImGuizmo::IsUsing()){
                    m_GizmoType = ImGuizmo::SCALE;
                }
                break;
            }
            default:
                break;
            }

            return false;
        });
    }
}
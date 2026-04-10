#include "EditorViewport.h"
#include "Editor/DSMEditor.h"
#include "Editor/Project.h"
#include "Editor/EditorUI/EditorSceneHierarchy.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Render/Mesh.h"
#include "Runtime/Event/KeyEvent.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Framework/Object/GameObject.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glfw/glfw3.h>

namespace DSM {
    EditorViewport::EditorViewport(EditorUI* editorUI)
        : Widget(editorUI)
    {
        m_Title = "Viewport";
        m_Size = Math::Vector2{400.0f, 300.0f};
        m_Flags |= ImGuiWindowFlags_NoScrollbar;
        m_Padding = Math::Vector2{2, 2};
        UpdateDpiScale();
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
            m_EditorUI->GetEditor()->SetShouldResizeRenderer(width, height);
        }

        // 设置渲染图像
        auto colorTex = renderer->GetColorTexture();
        // TODO: 后续更换为跨平台接口
        auto gpuHandle = colorTex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
        ImGui::Image(ImTextureRef{gpuHandle}, viewportSize);

        // 处理拖放资源到视口的情况
        if(ImGui::BeginDragDropTarget()){
            // 从资源管理器拖放文件到视口
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(Project::s_ContentBrowserDragDropPayload)){
                const char* path = static_cast<const char*>(payload->Data);
                auto filePath = std::filesystem::path(path);
                auto extension = filePath.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
                
                // 根据文件类型进行不同的处理 
                if(extension == Project::s_SceneFileExtension){
                    Project::GetInstance().LoadScene(path);
                }
                else if(Project::s_ModelFileExtensions.contains(extension.c_str())){
                    auto scene = DSMEngine::sm_GlobalContext.scene;
                    auto model = Model::LoadModel(path);
                    if(scene != nullptr && model != nullptr){
                        ConvertModelToGameObject(model, *scene);
                    }
                }
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
        auto transfrom = selectedObject->GetComponent<TransformComponent>();
        Math::Matrix4 transMat = transfrom->GetLocalToWorld();
        ImGuizmo::Manipulate((float*)&cameraView, (float*)&cameraProj, 
            static_cast<ImGuizmo::OPERATION>(m_GizmoType), ImGuizmo::LOCAL, (float*)&transMat);
        if(ImGuizmo::IsUsing()){
            *transfrom = TransformComponent(selectedObject, transMat);
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
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
            if(e.GetWidth() != 0 && e.GetHeight() != 0){
                UpdateDpiScale();
            }
            return false;
        });
    }
    
    void EditorViewport::UpdateDpiScale()
    {
        int window_width, window_height;
        auto window = DSMEngine::sm_GlobalContext.window->GetNativeWindow();
        glfwGetWindowSize(window, &window_width, &window_height);
        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)window_width, (float)window_height);
        io.DisplayFramebufferScale = ImVec2((float)fb_width / (float)window_width,
                                        (float)fb_height / (float)window_height);

        // 只缩放字体，不缩放样式
        static const float base_width = 1200.f;
        float scale = io.DisplaySize.x / base_width;
        io.FontGlobalScale = scale;
    }
}
#include "EditorUI.h"
#include "Editor/DSMEditor.h"
#include "Editor/SceneSerializer.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Event/KeyEvent.h"
#include "Runtime/Core/Input/InputSystem.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <ImGuizmo.h>

namespace DSM {
    EditorUI::EditorUI(const EditorUIDesc& desc)
        : m_SceneHierarchyPanel(std::make_unique<SceneHierarchyPanel>()),
        m_ContentBrowserPanel(std::make_unique<ContentBrowserPanel>())
        {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

        io.MouseDrawCursor = true;

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
        
        ImGui_ImplGlfw_InitForOther(desc.window->GetNativeWindow(), true);

        // 根据不同的图形 API 初始化不同的 ImGui 后端
        desc.renderer->InitWindowUI(this);

        m_SceneHierarchyPanel->SetScene(DSMEngine::sm_GlobalContext.scene);
    }

    void EditorUI::Render()
    {
        static bool dockspaceOpen = true;
        static bool opt_fullscreen = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (HasFlags(ImGuiDockNodeFlags_(dockspace_flags), ImGuiDockNodeFlags_PassthruCentralNode))
            window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        // Begin the dockspace window
        ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        // 开启菜单栏
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("New Scene")) {
                    NewScene();
                }
                if(ImGui::MenuItem("Save Scene")){
                    SaveScene();
                }
                if(ImGui::MenuItem("Load Scene")){
                    auto filepath = Utility::FileDialogs::OpenFile("DSM Engine Scene (*.dsmescene)\0*.dsmescene\0");
                    LoadScene(filepath);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingOverCentralNode) != 0))  {
                    DSMEditor::sm_EditorContext.engine->Close();
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }

        // End the dockspace window
        ImGui::End();

        RenderViewportWindow();

        DSMEngine::sm_GlobalContext.scene->OnGUI();
        m_SceneHierarchyPanel->OnGUI();
        m_ContentBrowserPanel->OnGUI();
    }

    void EditorUI::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
            if(e.IsRepeat()){
                return false;
            }
            
            bool control = DSMEngine::sm_GlobalContext.inputSystem->IsKeyPressed(KeyCode::LeftControl) || 
                DSMEngine::sm_GlobalContext.inputSystem->IsKeyPressed(KeyCode::RightControl);
            bool shift = DSMEngine::sm_GlobalContext.inputSystem->IsKeyPressed(KeyCode::LeftShift) || 
                DSMEngine::sm_GlobalContext.inputSystem->IsKeyPressed(KeyCode::RightShift);

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

    void EditorUI::RenderViewportWindow()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport");

        m_ViewportBounds = {
            ImGui::GetWindowPos().x,
            ImGui::GetWindowPos().y,
            ImGui::GetWindowSize().x,
            ImGui::GetWindowSize().y
        };
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        auto& renderer = DSMEditor::sm_EditorContext.renderer;
        Viewport cameraViewport = renderer->GetCamera().GetViewPort();
        if(cameraViewport.Width() != m_ViewportBounds.Get(2) ||
            cameraViewport.Height() != m_ViewportBounds.Get(3)){
            renderer->GetCamera().SetViewPort(Viewport{m_ViewportBounds.Get(2), m_ViewportBounds.Get(3)});
            renderer->ResizeRenderTexture(m_ViewportBounds.Get(2), m_ViewportBounds.Get(3));
        }

        auto colorTex = renderer->GetColorTexture();
        auto gpuHandle = colorTex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
        ImGui::Image(ImTextureRef{gpuHandle}, viewportSize);

        if(ImGui::BeginDragDropTarget()){
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ContentBrowserPanel::sm_DragDropPayloadType)){
                const char* path = static_cast<const char*>(payload->Data);
                LoadScene(path);
            }
            ImGui::EndDragDropTarget();
        }
        
        RenderGizmo();
        
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorUI::RenderGizmo()
    {
        auto selectedObject = m_SceneHierarchyPanel->GetSelectedObject().lock();
        if(selectedObject == nullptr || m_GizmoType == -1){
            return;
        }

        const auto& camera = DSMEditor::sm_EditorContext.renderer->GetCamera();

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        ImGuizmo::SetRect(m_ViewportBounds.Get(0), m_ViewportBounds.Get(1), 
            m_ViewportBounds.Get(2), m_ViewportBounds.Get(3));

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

    void EditorUI::NewScene()
    {
        DSMEngine::sm_GlobalContext.scene = std::make_shared<Scene>();
        m_SceneHierarchyPanel->SetScene(DSMEngine::sm_GlobalContext.scene);  
    }

    void EditorUI::SaveScene()
    {
        auto filepath = Utility::FileDialogs::SaveFile("DSM Engine Scene (*.dsmescene)\0*.dsmescene\0");
        if (!filepath.empty()) {
            SceneSerializer serializer;
            serializer.Serialize(filepath);
        }
    }

    void EditorUI::LoadScene(const std::filesystem::path& filepath)
    {
        if (filepath.extension().string() != ".dsmescene") {
            DSM_WARN("Could not load file {}, is not a scene file", filepath.filename().string());
			return;
		}

        if (!filepath.empty()) {
            SceneSerializer serializer;
            if(serializer.Deserialize(filepath.string())){
                m_SceneHierarchyPanel->SetScene(DSMEngine::sm_GlobalContext.scene);  
            }
        }
    }

} // namespace DSM
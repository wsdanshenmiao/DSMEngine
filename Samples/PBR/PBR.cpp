#include "Editor/DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Render/Camera/CameraController.h"
#include "Runtime/Render/Geometry.h"
#include "Passes/SetupPass.h"
#include "Passes/GeometryPass.h"
#include "Passes/LitPass.h"
#include "Passes/FinalPass.h"
#include "Passes/LightingPass.h"
#include "Passes/ShadowPass.h"
#include "Passes/SkyBoxPass.h"
#include "Passes/SSAO.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/ScriptableObject.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Framework/Component/Component.h"
#include <imgui.h>
#include <print>

using namespace DSM;


class RenderPipeline : public DSM::IRenderPipeline
{
public:
    ~RenderPipeline() override
    {
        g_RenderResources = {};
    }

    void Initialize(DSM::Renderer& renderer)
    {
        auto& globalContext = DSMEngine::sm_GlobalContext;

        auto lihuazou = ModelLoader::LoadModel("Models/AB/AliceADefault/AliceADefault.fbx");
        m_Models.push_back(lihuazou);
        
        auto plane = ModelLoader::LoadModelFromGeometry("Plane", Geometry::GeometryGenerator::CreateGrid(50,50,2,2));
        // plane->transform.SetPosition({0,-1,0});

        // auto cube = ModelLoader::LoadModelFromGeometry("Cube", Geometry::GeometryGenerator::CreateBox(6,4,4,2));
        // cube->transform.SetPosition({0,4,0});

        // auto sphere = ModelLoader::LoadModelFromGeometry(
        //     "Sphere", Geometry::GeometryGenerator::CreateSphere(100.0f, 16, 16));

        // auto house = ModelLoader::LoadModel("Models/house.obj");
        // house->transform.SetScale(0.01f);
        // m_Models.push_back(house);

        m_Models.push_back(plane);
        // m_Models.push_back(cube);

        
        std::shared_ptr<GameObject> lihuazouObj = globalContext.scene->GetObjectByID(
            globalContext.scene->CreateObject("Lihuazou")).lock();
        lihuazouObj->AddComponent<Model>(*lihuazou);
        lihuazouObj->GetComponent<Math::Transform>()->SetScale(2);
        std::shared_ptr<GameObject> planeObj = globalContext.scene->GetObjectByID(
            globalContext.scene->CreateObject("Plane")).lock();
        planeObj->AddComponent<Model>(*plane);
        
        auto sponza = ModelLoader::LoadModel("Models/Sponza/pbr/sponza2.gltf");
        m_Models.push_back(sponza);
        std::shared_ptr<GameObject> sponzaObj = globalContext.scene->GetObjectByID(
            globalContext.scene->CreateObject("Sponza")).lock();
        sponzaObj->AddComponent<Model>(*sponza);

        CreateLight();

        m_RenderPasses.push_back(std::make_unique<SetupPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<GeometryPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<SSAO>(renderer));
        m_RenderPasses.push_back(std::make_unique<ShadowPass>(renderer, ShadowSetting{}));
        m_RenderPasses.push_back(std::make_unique<LightingPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<LitPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<SkyboxPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<FinalPass>(renderer));

        auto& camera = renderer.GetCamera();
        camera.SetPosition({0, 3, 5});
        camera.LookAt({0,0,0}, {0,1,0});
        m_CameraController = std::make_unique<CameraController>();
        m_CameraController->InitCamera(&camera);
    }


    void Render(DSM::Renderer& renderer, float deltaTime) override
    {
        if (!m_Initialized) {
            // Initialize resources
            Initialize(renderer);
            m_Initialized = true;
        }
        // // 按下 R 键重新编译着色器
        // if(DSMEngine::sm_GlobalContext.inputSystem->IsKeyPressed(KeyCode::R)){
        //     auto setupPass= Utility::CheckedCast<SetupPass*>(m_RenderPasses[0].get());
        //     setupPass->CreateShader(renderer);
        // }

        m_CameraController->Update(deltaTime);

        std::vector<float> renderPassTimes;
        CpuTimer timer{};
        timer.Reset();
        timer.Start();
        for (auto& renderPass : m_RenderPasses) {
            renderPass->Render(renderer, deltaTime);
            timer.Tick();
            renderPassTimes.push_back(timer.DeltaTime() * 1000.f);
        }
        timer.Tick();

        auto infoTimer = [&]() {
            DSM_INFO("Frame {}:", renderer.GetFrameIndex());
            DSM_INFO("Shadow Pass Time: {} ms", renderer.GetDevice()->GetTimerQueryTime(ShadowPass::sm_TimerQuery) * 1000.f);
            DSM_INFO("Lighting Pass Time: {} ms", renderer.GetDevice()->GetTimerQueryTime(LightingPass::sm_TimerQuery) * 1000.f);
            DSM_INFO("Geometry Pass Time: {} ms", renderer.GetDevice()->GetTimerQueryTime(GeometryPass::sm_TimerQuery) * 1000.f);
            DSM_INFO("SSAO Pass Time: {} ms", renderer.GetDevice()->GetTimerQueryTime(SSAO::sm_TimerQuery) * 1000.f);
            DSM_INFO("Lit Pass Time: {} ms", renderer.GetDevice()->GetTimerQueryTime(LitPass::sm_TimerQuery) * 1000.f);
            DSM_INFO("Skybox Pass Time: {} ms", renderer.GetDevice()->GetTimerQueryTime(SkyboxPass::sm_TimerQuery) * 1000.f);
            DSM_INFO("Final Pass Time: {} ms", renderer.GetDevice()->GetTimerQueryTime(FinalPass::sm_TimerQuery) * 1000.f);
            for(size_t i = 0; i < renderPassTimes.size(); i++) {
                DSM_INFO("Render Pass {} Time: {} ms", i, renderPassTimes[i]);
            }
            DSM_INFO("Cpu Time: {} ms", timer.DeltaTime() * 1000.f);
            DSM_INFO("---------------------------------------------------");
        };
        //infoTimer();
        timer.Stop();

    }

    void RenderUI(DSM::Renderer& renderer) override
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
            if (ImGui::BeginMenu("Options")) {
                if (ImGui::MenuItem("Exit", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingOverCentralNode) != 0))  {
                    DSMEditor::sm_EditorContext.engine->Close();
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }

        ImGui::End();

        
        // Viewport windows
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
            ImGui::Begin("Viewport");

            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            Viewport cameraViewport = renderer.GetCamera().GetViewPort();
            if(cameraViewport.Width() != viewportSize.x ||
               cameraViewport.Height() != viewportSize.y){
                renderer.GetCamera().SetViewPort(Viewport{viewportSize.x, viewportSize.y});
                for (auto& renderPass : m_RenderPasses) {
                    renderPass->OnResize(renderer, (uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
                }
            }

            auto colorTex = GetCommonTexture(CommonTextureSlot::Color);
            auto gpuHandle = colorTex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
            ImGui::Image(ImTextureRef{gpuHandle}, viewportSize);

            ImGui::End();
            ImGui::PopStyleVar();
        }


        static float lightDir[3] = {-0.3, -1, 0.08};
        static float lightColor[3] = {1.0f, 1.0f, 1.0f};
        if (ImGui::Begin("Light Settings")) {
            ImGui::SliderFloat3("Light Direction", lightDir, -1.0f, 1.0f);
            ImGui::ColorEdit3("Light Color", lightColor);

            static const char* pcfMode[] = {
                "None",
                "3x PCF",
                "5x PCF",
                "7x PCF"
            };
            static int curr_scene_pcf_item = ShadowPass::sm_Setting.directionalSetting.filter;
            if (ImGui::Combo("Scene PCF", &curr_scene_pcf_item, pcfMode, ARRAYSIZE(pcfMode))) {
                auto& filter = ShadowPass::sm_Setting.directionalSetting.filter;
                auto currMode = ShadowSetting::FilterMode(curr_scene_pcf_item);
                if(filter != currMode){
                    const auto& shaders = g_RenderResources.shaders;
                    for(auto& config : g_RenderResources.renderConfigs){
                        size_t baseIndex = config.pipelineDesc.PS == shaders[(size_t)ShaderSlot::LitPS + filter] ? 
                            (size_t)ShaderSlot::LitPS : (size_t)ShaderSlot::LitPSNoTangent;
                        config.pipelineDesc.PS = shaders[baseIndex + currMode];
                    }
                    filter = currMode;
                }
            }

            ImGui::Checkbox("Enable SSAO", &SSAO::sm_Settings.enable);
            if(SSAO::sm_Settings.enable){
                ImGui::SliderFloat("Occlusion Radius", &SSAO::sm_Settings.occlusionRadius, 0.1f, 2.0f);
                ImGui::SliderInt("Sample Count", (int*)&SSAO::sm_Settings.sampleCount, 1, 14);
                ImGui::SliderFloat("SSAO Threshold", &SSAO::sm_Settings.occlusionThreshold, 0.001f, 0.5f);
                ImGui::SliderFloat("SSAO Fade", &SSAO::sm_Settings.fadeEnd, 1.f, 5.0f);
                ImGui::SliderInt("SSAO Contrast", (int*)&SSAO::sm_Settings.contrast, 1, 5);
                ImGui::SliderInt("SSAO Blur Radius", (int*)&SSAO::sm_Settings.blurRadius, 0, 5);
                ImGui::SliderInt("SSAO Blur Count", (int*)&SSAO::sm_Settings.blurCount, 1, 5);
            }
        }
        ImGui::End();

        if(auto lights = g_RenderResources.lights; !lights.empty() && 
           lights[0].lightType == LightType::Directional){
            g_RenderResources.lights[0].direction = {lightDir[0], lightDir[1], lightDir[2]};
            g_RenderResources.lights[0].color = {lightColor[0], lightColor[1], lightColor[2], 1.0f};
        }
    }

    void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
    {
        // for (auto& renderPass : m_RenderPasses) {
        //     renderPass->OnResize(renderer, width, height);
        // }
    }

private:
    void CreateLight()
    {
        g_RenderResources.lights.push_back(Light{}
            .SetType(LightType::Directional)
            .SetDirection(Math::Vector3{-0.3f, -1.0f, -0.2f}.Normalized())
            .SetColor({1,1,1,1}));

        auto randUint = [](){
            return rand(); // [0, RAND_MAX]
        };
        auto randFloat = [randUint]() -> float {
            return randUint() * (1.0f / RAND_MAX); // convert [0, RAND_MAX] to [0, 1]
        };
        auto randVecUniform = [randFloat]() {
            return Math::Vector3{randFloat(), randFloat(), randFloat()};
        };
        auto randGaussian = [randFloat]() {
            // polar box-muller
            static bool gaussianPair = true;
            static float y2;

            if (gaussianPair) {
                gaussianPair = false;

                float x1, x2, w;
                do {
                    x1 = 2 * randFloat() - 1;
                    x2 = 2 * randFloat() - 1;
                    w = x1 * x1 + x2 * x2;
                } while (w >= 1);

                w = sqrtf(-2 * logf(w) / w);
                y2 = x2 * w;
                return x1 * w;
            }
            else {
                gaussianPair = true;
                return y2;
            }
        };
        auto randVecGaussian = [randGaussian]() -> Math::Vector3{
            return Math::Vector3{randGaussian(), randGaussian(), randGaussian()}.Normalized();
        };

        const float pi = 3.14159265359f;
        Math::Vector3 posScale{40, 15, 10};
        Math::Vector3 posBias{-4, 2, 0};
        for (uint32_t n = 0; n < LightingPass::sm_MaxOtherLightCount; n++)
        {
            Math::Vector3 pos = randVecUniform() * posScale + posBias;
            float lightRadius = randFloat() * 800.0f + 200.0f;

            Math::Vector3 color = randVecUniform();
            float colorScale = randFloat() * .3f + .3f;
            color = color * colorScale;

            uint32_t type;
            // force types to match 32-bit boundaries for the BIT_MASK_SORTED case
            if (n < 32 * 2)
                type = 1;
            else
                type = 2;

            Math::Vector3 coneDir = randVecGaussian();
            float coneInner = (randFloat() * .2f + .025f) * pi;
            float coneOuter = coneInner + randFloat() * .1f * pi;
            
            Light light{};
            light.SetType(LightType(type))
                .SetPosition(pos)
                .SetRange(lightRadius)
                .SetColor(Math::Vector4{color})
                .SetDirection(coneDir)
                .SetInnerAngle(coneInner)
                .SetOuterAngle(coneOuter);
            
            g_RenderResources.lights.push_back(std::move(light));
        }
    }


private:
    bool m_Initialized = false;

    std::vector<std::shared_ptr<Model>> m_Models;
    std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;

    std::unique_ptr<CameraController> m_CameraController;
};

int main()
{
    DSM::DSMEngine engine;
    DSM::EngineParameters params{};
    params.enableDebugLayer = true;
    engine.StartEngine(params);
    engine.SetRenderPipeline(std::make_unique<RenderPipeline>());

    DSM::DSMEditor editor(&engine);
    editor.Run();

    engine.ShutDownEngine();

    return 0;
}
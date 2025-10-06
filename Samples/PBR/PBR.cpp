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
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/DSMEngine.h"
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
        auto lihuazou = ModelLoader::LoadModel("Models/AB/AliceADefault/AliceADefault.fbx");
        lihuazou->transform.SetScale(2);
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

        auto sponza = ModelLoader::LoadModel("Models/Sponza/pbr/sponza2.gltf");
        m_Models.push_back(sponza);
        m_Models.push_back(plane);
        // m_Models.push_back(cube);

        auto dirLight = Light{
            .lightType = LightType::Directional, 
            .color = Math::Vector4{1,1,1,1}, 
            .range = 10.0f};
        dirLight.direction = {-0.3, -1, 0.08};
        g_RenderResources.lights.push_back(dirLight);
        dirLight.direction.Set(0, 0.3);
        g_RenderResources.lights.push_back(dirLight);

        m_RenderPasses.push_back(std::make_unique<SetupPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<ShadowPass>(renderer, ShadowSetting{}, m_Models));
        m_RenderPasses.push_back(std::make_unique<LightingPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<GeometryPass>(renderer, m_Models));
        m_RenderPasses.push_back(std::make_unique<SSAO>(renderer));
        m_RenderPasses.push_back(std::make_unique<LitPass>(renderer, m_Models));
        m_RenderPasses.push_back(std::make_unique<SkyboxPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<FinalPass>(renderer, m_Models));

        auto& camera = renderer.GetCamera();
        camera.SetPosition(dirLight.direction * -5.0f);
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

        g_RenderResources.lights[0].direction = {lightDir[0], lightDir[1], lightDir[2]};
        g_RenderResources.lights[0].color = {lightColor[0], lightColor[1], lightColor[2], 1.0f};
    }

    void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
    {
        for (auto& renderPass : m_RenderPasses) {
            renderPass->OnResize(renderer, width, height);
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
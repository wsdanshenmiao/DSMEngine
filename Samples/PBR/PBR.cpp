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
        for (auto& renderPass : m_RenderPasses) {
            renderPass->OnResize(renderer, width, height);
        }
    }

private:
    void CreateLight()
    {
        g_RenderResources.lights.push_back(Light{}
            .SetType(LightType::Directional)
            .SetDirection(Math::Vector3{-0.5f, -0.8f, -0.5f}.Normalized())
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

    DSM::DSMEditor editor{};
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();

    engine.ShutDownEngine();

    return 0;
}
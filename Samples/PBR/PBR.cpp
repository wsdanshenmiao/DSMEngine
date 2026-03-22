#include "Editor/DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Render/Camera/CameraController.h"
#include "Runtime/Render/Geometry.h"
#include "Runtime/Render/Renderer/ForwardRenderer/SetupPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/GeometryPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/LitPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/FinalPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/LightingPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/ShadowPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/SkyBoxPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/SSAOPass.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/ScriptableObject.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Core/InstrumentorTimer.h"
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
        m_RenderPasses.push_back(std::make_unique<SSAOPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<ShadowPass>(renderer, ShadowSetting{}));
        m_RenderPasses.push_back(std::make_unique<LightingPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<LitPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<SkyboxPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<FinalPass>(renderer));

        auto& camera = renderer.GetCamera();
        camera.SetPosition({0, 3, -5});
        camera.LookAt({0,0,0}, {0,1,0});
        m_CameraController = std::make_unique<CameraController>();
        m_CameraController->InitCamera(&camera);

        auto scene = DSMEngine::sm_GlobalContext.scene;
        auto processModel = [scene](std::shared_ptr<GameObject> obj, const std::shared_ptr<Model>& model) {
            for (const auto& mesh : model->meshes) {
                auto subObj = scene->CreateObject(mesh->name);
                auto subObjPtr = scene->GetObjectByID(subObj).lock();
                subObjPtr->AddComponent<Mesh>(*mesh);
                subObjPtr->AddComponent<Math::AxisAlignedBox>(mesh->boundingBox);
                subObjPtr->AddComponent<ShaderResource::MaterialData>(*model->materials[mesh->materialIndex]);
                obj->AddChild(subObjPtr);
            }
        };
        auto lihuazou = scene->CreateObject("Lihuazou");
        auto lihuazouPtr = scene->GetObjectByID(lihuazou).lock();
        auto lihuazouModel = ModelLoader::LoadModel("Models/AB/AliceADefault/AliceADefault.fbx");
        processModel(lihuazouPtr, lihuazouModel);

		auto sponza = scene->CreateObject("Sponza");
		auto sponzaPtr = scene->GetObjectByID(sponza).lock();
        auto sponzaModel = ModelLoader::LoadModel("Models/Sponza/pbr/sponza2.gltf");
        processModel(sponzaPtr, sponzaModel);
    }


    void Render(DSM::Renderer& renderer, float deltaTime) override
    {
        if (!m_Initialized) {
            // Initialize resources
            Initialize(renderer);
            m_Initialized = true;
        }

        m_CameraController->Update(deltaTime);

        for (auto& renderPass : m_RenderPasses) {
            renderPass->Render(renderer, deltaTime);
        }
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
                filter = ShadowSetting::FilterMode(curr_scene_pcf_item);
            }

            ImGui::Checkbox("Enable SSAO", &SSAOPass::sm_Settings.enable);
            if(SSAOPass::sm_Settings.enable){
                ImGui::SliderFloat("Occlusion Radius", &SSAOPass::sm_Settings.occlusionRadius, 0.1f, 2.0f);
                ImGui::SliderInt("Sample Count", (int*)&SSAOPass::sm_Settings.sampleCount, 1, 14);
                ImGui::SliderFloat("SSAO Threshold", &SSAOPass::sm_Settings.occlusionThreshold, 0.001f, 0.5f);
                ImGui::SliderFloat("SSAO Fade", &SSAOPass::sm_Settings.fadeEnd, 1.f, 5.0f);
                ImGui::SliderInt("SSAO Contrast", (int*)&SSAOPass::sm_Settings.contrast, 1, 5);
                ImGui::SliderInt("SSAO Blur Radius", (int*)&SSAOPass::sm_Settings.blurRadius, 0, 5);
                ImGui::SliderInt("SSAO Blur Count", (int*)&SSAOPass::sm_Settings.blurCount, 1, 5);
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
            .SetDirection(Math::Vector3{0.5f, -0.8f, 0.5f}.Normalized())
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
    Instrumentor::BeginSession("PBR Profiling");
    DSM::DSMEngine engine;
    DSM::EngineParameters params{};
    params.enableDebugLayer = false;
    engine.StartEngine(params);
    engine.SetRenderPipeline(std::make_unique<RenderPipeline>());

    DSM::DSMEditor editor{};
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();

    engine.ShutDownEngine();

    return 0;
}
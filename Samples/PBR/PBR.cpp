#include "Editor/DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Render/Camera/CameraController.h"
#include "Passes/SetupPass.h"
#include "Passes/GeometryPass.h"
#include "Passes/FinalPass.h"
#include "Passes/LightingPass.h"
#include "Runtime/Render/Geometry.h"
#include <imgui.h>

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
        auto model = ModelLoader::LoadModel("Models/Sponza/sponza.gltf");
        // auto sphere = Geometry::GeometryGenerator::CreateSphere(100.0f, 16, 16);
        // auto model = ModelLoader::LoadModelFromGeometry("Sphere", sphere);
        assert(model != nullptr);
        m_Models.push_back(model);

        auto dirLight = Light{
            .lightType = LightType::Directional, 
            .color = Math::Vector4{1,1,1,1}, 
            .range = 10.0f};
        dirLight.transform.LookTo({-0.8, -1, 0.8});
        g_RenderResources.lights.push_back(dirLight);

        m_RenderPasses.push_back(std::make_unique<SetupPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<LightingPass>(renderer));
        m_RenderPasses.push_back(std::make_unique<GeometryPass>(renderer, m_Models));
        m_RenderPasses.push_back(std::make_unique<FinalPass>(renderer, m_Models));

        renderer.GetCamera().SetPosition(0, 0, -5);
        m_CameraController = std::make_unique<CameraController>();
        m_CameraController->InitCamera(&renderer.GetCamera());
    }


    void Render(DSM::Renderer& renderer, float deltaTime) override
    {
        if (!m_Initialized) {
            // Initialize resources
            Initialize(renderer);
            m_Initialized = true;
        }
        m_CameraController->Update(deltaTime);

        auto cmdList = renderer.GetDevice()->CreateCommandList();
        cmdList->Open();

        cmdList->Close();
        renderer.GetDevice()->ExecuteCommandList(cmdList);

        for (auto& renderPass : m_RenderPasses) {
            renderPass->Render(renderer, deltaTime);
        }
    }

    void RenderUI(DSM::Renderer& renderer) override
    {
        static float baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        static float emissiveColor[3] = {0.0f, 0.0f, 0.0f};
        static float metallicFactor = 0.5f;
        static float roughnessFactor = 0.5f;
        if (ImGui::Begin("Material Settings")) {
            ImGui::ColorEdit4("Base Color", baseColor);
            ImGui::ColorEdit3("Emissive Color", emissiveColor);
            ImGui::SliderFloat("Metallic Factor", &metallicFactor, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness Factor", &roughnessFactor, 0.0f, 1.0f);
        }
        ImGui::End();
        Material material{};
        material.baseColor = Math::Vector4{baseColor[0], baseColor[1], baseColor[2], baseColor[3]};
        material.emissiveColor = Math::Vector3{emissiveColor[0], emissiveColor[1], emissiveColor[2]};
        material.metallicFactor = metallicFactor;
        material.roughnessFactor = roughnessFactor;

        auto cmdList = renderer.GetDevice()->CreateCommandList();
        cmdList->Open();
        for (const auto& model : m_Models) {
            auto matByteSize = Math::Align(sizeof(Material), size_t(c_ConstantBufferOffsetSizeAlignment));
            for (size_t i = 0; i < model->materials.size(); i++) {
                cmdList->WriteBuffer(model->materialData, &material, sizeof(Material), i * matByteSize);
            }
        }
        cmdList->Close();
        renderer.GetDevice()->ExecuteCommandList(cmdList);
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
    engine.StartEngine();
    engine.SetRenderPipeline(std::make_unique<RenderPipeline>());

    DSM::DSMEditor editor(&engine);
    editor.Run();

    engine.ShutDownEngine();

    return 0;
}
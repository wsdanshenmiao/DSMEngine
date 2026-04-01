#pragma once
#ifndef __FORWARDRENDERPIPELINE_H__
#define __FORWARDRENDERPIPELINE_H__

#include "RenderResource.h"
#include "GeometryPass.h"
#include "SSAOPass.h"
#include "ShadowPass.h"
#include "LightingPass.h"
#include "LitPass.h"
#include "SkyboxPass.h"
#include "FinalPass.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Render/Camera/CameraController.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include "Runtime/Render/Renderer/ForwardRenderer/PostEffect/PostEffectManager.h"

#include <imgui.h>

namespace DSM {
    

    class ForwardRenderPipeline : public IRenderPipeline
    {
    public:
        ForwardRenderPipeline()
        {
            DSM_ASSERT(DSMEngine::sm_GlobalContext.renderer != nullptr, "Renderer must be initialized before creating ForwardRenderPipeline");
            auto& renderer = *DSMEngine::sm_GlobalContext.renderer;
            RenderResource::Create(renderer.GetDevice());
            auto& backBufferDesc = renderer.GetCurrentBackBuffer()->GetDesc();
            RenderResource::GetInstance().OnResize(renderer, backBufferDesc.width, backBufferDesc.height);

            CreateLight();

            m_RenderPasses.push_back(std::make_unique<GeometryPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<SSAOPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<ShadowPass>(renderer, ShadowSetting{}));
            m_RenderPasses.push_back(std::make_unique<LightingPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<LitPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<SkyboxPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<LitPass>(renderer, true)); // 透明物体
            m_RenderPasses.push_back(std::make_unique<PostEffectManager>(renderer));
            m_RenderPasses.push_back(std::make_unique<FinalPass>(renderer));

            auto& camera = renderer.GetCamera();
            camera.SetPosition({0, 2, -2});
            camera.LookAt({0,1,0}, {0,1,0});
            m_CameraController = std::make_unique<CameraController>();
            m_CameraController->InitCamera(&camera);

            // TODO: 这里先创建一个测试用的场景，后续应该将场景的创建放到外部，由用户根据需要创建不同的场景
            auto scene = DSMEngine::sm_GlobalContext.scene;
            
            auto processModel = [scene](std::shared_ptr<GameObject> obj, const std::shared_ptr<Model>& model) {
                for (const auto& mesh : model->meshes) {
                    auto subObj = scene->CreateObject(mesh->name);
                    auto subObjPtr = scene->GetObjectByID(subObj).lock();
                    subObjPtr->AddComponent<Mesh>(*mesh);
                    subObjPtr->AddComponent<Math::AxisAlignedBox>(mesh->boundingBox);
                    subObjPtr->AddComponent<ShaderResource::MaterialData>(*model->materials[mesh->materialIndex]);
                    if(obj != nullptr){
                        obj->AddChild(subObjPtr);
                    }
                }
            };

            // 透明物体
            auto transparentTex = TextureManager::LoadTextureFromFile("Assets/Textures/transparent_texture.psd");
            auto boxMesh = Geometry::GeometryGenerator::CreateBox(1, 1, 1, 0);
            auto boxModel = ModelLoader::LoadModelFromGeometry("TransparentBox", boxMesh);
            boxModel->meshes[0]->psoFlags |= uint32_t(PSOFlags::kAlphaBlend);
            boxModel->meshes[0]->psoFlags |= uint32_t(PSOFlags::kBothSide);
            boxModel->meshes[0]->textures[ShaderResource::kBaseColor] = transparentTex;
            boxModel->meshes[0]->textures[ShaderResource::kEmissive] = transparentTex;
            boxModel->materials[0]->emissiveColor = {0.8, 0.8, 0.8, 1};
            processModel(nullptr, boxModel);
            auto transparentObj = scene->GetObjectsWithComponents<Mesh>();
            for(auto [id, mesh] : transparentObj.each()){
				auto obj = scene->GetObjectByID(id).lock();
                if(obj != nullptr){
					auto transform = obj->GetComponent<Math::Transform>();
                    transform->SetPosition({-2, 0.5f, 0});
                    transform->SetRotation({0, 45, 0});
				}
			}

            // 不透明物体
            auto lihuazou = scene->CreateObject("Lihuazou");
            auto lihuazouPtr = scene->GetObjectByID(lihuazou).lock();
            auto lihuazouModel = ModelLoader::LoadModel("Assets/Models/AB/AliceADefault/AliceADefault.fbx");
            processModel(lihuazouPtr, lihuazouModel);

            auto sponza = scene->CreateObject("Sponza");
            auto sponzaPtr = scene->GetObjectByID(sponza).lock();
            auto sponzaModel = ModelLoader::LoadModel("Assets/Models/Sponza/pbr/sponza2.gltf");
            processModel(sponzaPtr, sponzaModel);
        }

        ~ForwardRenderPipeline() override
        {
            RenderResource::Destroy();
        }


        void Render(DSM::Renderer& renderer, float deltaTime) override
        {
            m_CameraController->Update(deltaTime);
            InstrumentationTimer timer0{"Update Render Resource"};
            RenderResource::GetInstance().UpdateRenderResource(renderer.GetCamera());
            timer0.Stop();
            for (auto [index, renderPass] : m_RenderPasses | std::views::enumerate) {
                InstrumentationTimer timer0{typeid(*renderPass).name()};
                uint64_t passFrameTime = renderPass->Render(renderer, deltaTime);
                RenderResource::GetInstance().SetRenderPassFinishFence(RenderPass(index), passFrameTime);
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

            auto lights = DSMEngine::sm_GlobalContext.scene->GetObjectsWithComponents<Light>();
            for(auto [id, light] : lights.each()){
                if(light.lightType == LightType::Directional){
                    light.direction = {lightDir[0], lightDir[1], lightDir[2]};
                    light.color = {lightColor[0], lightColor[1], lightColor[2], 1.0f};
                    break; // only update the first directional light
                }
            }
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            RenderResource::GetInstance().OnResize(renderer, width, height);
            for (auto& renderPass : m_RenderPasses) {
                renderPass->OnResize(renderer, width, height);
            }
        }

    private:
        void CreateLight()
        {
            auto scene = DSMEngine::sm_GlobalContext.scene;
            auto lightsObject = scene->CreateObject("Lights");
            auto dirLightPtr = scene->GetObjectByID(scene->CreateObject("Directional Light")).lock();
            dirLightPtr->AddComponent<Light>()
                ->SetType(LightType::Directional)
                .SetDirection(Math::Vector3{0.5f, -0.8f, 0.5f}.Normalized())
                .SetColor({1,1,1,1});
            dirLightPtr->SetParent(lightsObject);

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
                
                auto otherLight = scene->CreateObject(typeid(Light).name() + std::to_string(n));
                auto otherLightPtr = scene->GetObjectByID(otherLight).lock();
                otherLightPtr->AddComponent<Light>(light);
                otherLightPtr->SetParent(lightsObject);
            }
        }


    private:
        std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
        std::unique_ptr<CameraController> m_CameraController;
    };
}

#endif // __FORWARDRENDERPIPELINE_H__
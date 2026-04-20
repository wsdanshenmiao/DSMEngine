#pragma once
#ifndef __FORWARDRENDERPIPELINE_H__
#define __FORWARDRENDERPIPELINE_H__

#include "RenderResource.h"
#include "GeometryPass.h"
#include "SSAOPass.h"
#include "LightingPass.h"
#include "LitPass.h"
#include "SkyboxPass.h"
#include "TaaPass.h"
#include "FinalPass.h"
#include "MotionVectorPass.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Render/Renderer/ForwardRenderer/PostEffect/PostEffectManager.h"
#include "Runtime/Render/Camera/CameraController.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Core/InstrumentorTimer.h"

#include <imgui.h>

namespace DSM {
    

    class ForwardRenderPipeline : public IRenderPipeline
    {
    public:
        ForwardRenderPipeline()
        {
            DSM_CORE_ASSERT(DSMEngine::sm_GlobalContext.renderer != nullptr, "Renderer must be initialized before creating ForwardRenderPipeline");
            auto& renderer = *DSMEngine::sm_GlobalContext.renderer;
            RenderResource::Create(renderer.GetDevice());
            auto& backBufferDesc = renderer.GetCurrentBackBuffer()->GetDesc();
            RenderResource::GetInstance().OnResize(renderer, backBufferDesc.width, backBufferDesc.height);

            m_RenderPasses.push_back(std::make_unique<GeometryPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<MotionVectorPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<SSAOPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<LightingPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<LitPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<SkyboxPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<LitPass>(renderer, true)); // 透明物体
            m_RenderPasses.push_back(std::make_unique<TaaPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<PostEffectManager>(renderer));
            m_RenderPasses.push_back(std::make_unique<FinalPass>(renderer));

            auto& camera = renderer.GetCamera();
            m_CameraController = std::make_unique<CameraController>();
            m_CameraController->InitCamera(&camera);
        }

        ~ForwardRenderPipeline() override
        {
            RenderResource::Destroy();
        }


        void Render(DSM::GraphicsRenderer& renderer, float deltaTime) override
        {
            m_CameraController->Update(deltaTime);
            InstrumentationTimer timer0{"Update Render Resource"};
            RenderResource::GetInstance().UpdateRenderResource(renderer.GetCamera());
            timer0.Stop();
            for (auto [index, renderPass] : m_RenderPasses | std::views::enumerate) {
                InstrumentationTimer timer{typeid(*renderPass).name()};
                uint64_t passFrameTime = renderPass->Render(renderer, deltaTime);
                RenderResource::GetInstance().SetRenderPassFinishFence(RenderPass(index), passFrameTime);
            }
        }

        void RenderUI(DSM::GraphicsRenderer& renderer) override
        {
            if(ImGui::Begin("Camera Settings")){
                float cameraSpeed = m_CameraController->GetMoveSpeed();
                if(ImGui::SliderFloat("Camera Move Speed", &cameraSpeed, 0.1f, 10.0f)){
                    m_CameraController->SetMoveSpeed(cameraSpeed);
                }
            }
            ImGui::End();

            // static float lightDir[3] = {-0.3, -1, 0.08};
            // static float lightColor[3] = {1.0f, 1.0f, 1.0f};
            // if (ImGui::Begin("Light Settings")) {
            //     ImGui::SliderFloat3("Light Direction", lightDir, -1.0f, 1.0f);
            //     ImGui::ColorEdit3("Light Color", lightColor);

            //     static const char* pcfMode[] = {
            //         "None",
            //         "3x PCF",
            //         "5x PCF",
            //         "7x PCF"
            //     };
            //     static int curr_scene_pcf_item = Shadows::sm_Setting.directionalSetting.filter;
            //     if (ImGui::Combo("Scene PCF", &curr_scene_pcf_item, pcfMode, ARRAYSIZE(pcfMode))) {
            //         auto& filter = Shadows::sm_Setting.directionalSetting.filter;
            //         filter = ShadowSetting::FilterMode(curr_scene_pcf_item);
            //     }

            //     ImGui::Checkbox("Enable SSAO", &SSAOPass::sm_Settings.enable);
            //     if(SSAOPass::sm_Settings.enable){
            //         ImGui::SliderFloat("Occlusion Radius", &SSAOPass::sm_Settings.occlusionRadius, 0.1f, 2.0f);
            //         ImGui::SliderInt("Sample Count", (int*)&SSAOPass::sm_Settings.sampleCount, 1, 14);
            //         ImGui::SliderFloat("SSAO Threshold", &SSAOPass::sm_Settings.occlusionThreshold, 0.001f, 0.5f);
            //         ImGui::SliderFloat("SSAO Fade", &SSAOPass::sm_Settings.fadeEnd, 1.f, 5.0f);
            //         ImGui::SliderInt("SSAO Contrast", (int*)&SSAOPass::sm_Settings.contrast, 1, 5);
            //         ImGui::SliderInt("SSAO Blur Radius", (int*)&SSAOPass::sm_Settings.blurRadius, 0, 5);
            //         ImGui::SliderInt("SSAO Blur Count", (int*)&SSAOPass::sm_Settings.blurCount, 1, 5);
            //     }
            // }
            // ImGui::End();

            // auto lights = DSMEngine::sm_GlobalContext.scene->GetObjectsWithComponents<Light>();
            // for(auto [id, light] : lights.each()){
            //     if(light.GetType() == LightType::Directional){
            //         light.SetDirection({lightDir[0], lightDir[1], lightDir[2]});
            //         light.SetColor({lightColor[0], lightColor[1], lightColor[2], 1.0f});
            //         break; // only update the first directional light
            //     }
            // }
        }

        void OnResizeRenderTexture(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            RenderResource::GetInstance().OnResize(renderer, width, height);
            for (auto& renderPass : m_RenderPasses) {
                renderPass->OnResize(renderer, width, height);
            }
        }

        void OnResizeFrameBuffer(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override {}

    private:
        void CreateLight()
        {
            auto scene = DSMEngine::sm_GlobalContext.scene;
            auto lightsObject = scene->CreateObject("Lights");
            auto dirLightPtr = scene->GetObjectByID(scene->CreateObject("Directional Light")).lock();
            auto& dirLight = *dirLightPtr->AddComponent<Light>();
            dirLight.SetType(LightType::Directional)
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
                
                auto otherLight = scene->CreateObject(typeid(Light).name() + std::to_string(n));
                auto otherLightPtr = scene->GetObjectByID(otherLight).lock();
                auto& light = *otherLightPtr->AddComponent<Light>();
                light.SetType(LightType(type))
                    .SetPosition(pos)
                    .SetRange(lightRadius)
                    .SetColor(Math::Vector4{color})
                    .SetDirection(coneDir)
                    .SetInnerAngle(coneInner)
                    .SetOuterAngle(coneOuter);
                
                otherLightPtr->SetParent(lightsObject);
            }
        }

    private:
        std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
        std::unique_ptr<CameraController> m_CameraController;
    };
}

#endif // __FORWARDRENDERPIPELINE_H__

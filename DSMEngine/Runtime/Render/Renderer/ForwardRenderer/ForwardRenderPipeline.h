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
#include "TaaPass.h"
#include "FinalPass.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Render/Camera/CameraController.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include "Runtime/Render/Renderer/ForwardRenderer/PostEffect/PostEffectManager.h"

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
            m_RenderPasses.push_back(std::make_unique<SSAOPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<ShadowPass>(renderer, ShadowSetting{}));
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
                InstrumentationTimer timer0{typeid(*renderPass).name()};
                uint64_t passFrameTime = renderPass->Render(renderer, deltaTime);
                RenderResource::GetInstance().SetRenderPassFinishFence(RenderPass(index), passFrameTime);
            }
        }

        void RenderUI(DSM::GraphicsRenderer& renderer) override
        {
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
            //     static int curr_scene_pcf_item = ShadowPass::sm_Setting.directionalSetting.filter;
            //     if (ImGui::Combo("Scene PCF", &curr_scene_pcf_item, pcfMode, ARRAYSIZE(pcfMode))) {
            //         auto& filter = ShadowPass::sm_Setting.directionalSetting.filter;
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
        std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
        std::unique_ptr<CameraController> m_CameraController;
    };
}

#endif // __FORWARDRENDERPIPELINE_H__

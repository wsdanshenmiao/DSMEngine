#pragma once
#ifndef __DEFERRED_RENDER_PIPELINE_H__
#define __DEFERRED_RENDER_PIPELINE_H__

#include "GBufferPass.h"
#include "DeferredLightingPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/LitPass.h"
#include "Runtime/Render/Renderer/CommonPass/RenderResource.h"
#include "Runtime/Render/Renderer/CommonPass/MotionVectorPass.h"
#include "Runtime/Render/Renderer/CommonPass/SSAOPass.h"
#include "Runtime/Render/Renderer/CommonPass/LightingPass.h"
#include "Runtime/Render/Renderer/CommonPass/SkyboxPass.h"
#include "Runtime/Render/Renderer/CommonPass/TaaPass.h"
#include "Runtime/Render/Renderer/CommonPass/FinalPass.h"
#include "Runtime/Render/Renderer/CommonPass/PostEffect/PostEffectManager.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Render/Camera/CameraController.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include <imgui.h>

namespace DSM {

    class DeferredRenderPipeline : public IRenderPipeline
    {
    public:
        DeferredRenderPipeline()
        {
            DSM_CORE_ASSERT(DSMEngine::sm_GlobalContext.renderer != nullptr,
                "Renderer must be initialized before creating DeferredRenderPipeline");
            auto& renderer = *DSMEngine::sm_GlobalContext.renderer;
            RenderResource::Create(renderer.GetDevice());
            auto& backBufferDesc = renderer.GetCurrentBackBuffer()->GetDesc();
            RenderResource::GetInstance().OnResize(renderer, backBufferDesc.width, backBufferDesc.height);

            // Deferred-specific passes
            m_RenderPasses.push_back(std::make_unique<GBufferPass>(renderer));
            // Shared passes
            m_RenderPasses.push_back(std::make_unique<MotionVectorPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<SSAOPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<LightingPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<DeferredLightingPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<SkyboxPass>(renderer));
            // Transparent objects rendered forward
            m_RenderPasses.push_back(std::make_unique<LitPass>(renderer, true));
            m_RenderPasses.push_back(std::make_unique<TaaPass>(renderer));
            m_RenderPasses.push_back(std::make_unique<PostEffectManager>(renderer));
            m_RenderPasses.push_back(std::make_unique<FinalPass>(renderer));

            auto& camera = renderer.GetCamera();
            m_CameraController = std::make_unique<CameraController>();
            m_CameraController->InitCamera(&camera);
        }

        ~DeferredRenderPipeline() override
        {
            RenderResource::Destroy();
        }

        void Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            m_CameraController->Update(deltaTime);
            {
                InstrumentationTimer timer0{"Update Render Resource"};
                RenderResource::GetInstance().UpdateRenderResource(renderer.GetCamera());
            }
            for (auto [index, renderPass] : m_RenderPasses | std::views::enumerate) {
                InstrumentationTimer timer{typeid(*renderPass).name()};
                uint64_t passFrameTime = renderPass->Render(renderer, deltaTime);
                // Map pipeline pass index to RenderPass enum for fence tracking
                RenderPass passEnum = GetRenderPassEnum(static_cast<uint32_t>(index));
                RenderResource::GetInstance().SetRenderPassFinishFence(passEnum, passFrameTime);
                // 兼容：SSAOPass 引用 RenderPass::Geometry fence，同步 GBuffer fence 值
                if (passEnum == RenderPass::GBuffer) {
                    RenderResource::GetInstance().SetRenderPassFinishFence(RenderPass::Geometry, passFrameTime);
                }
            }
        }

        void RenderUI(GraphicsRenderer& renderer) override
        {
            if (ImGui::Begin("Camera Settings")) {
                float cameraSpeed = m_CameraController->GetMoveSpeed();
                if (ImGui::SliderFloat("Camera Move Speed", &cameraSpeed, 0.1f, 10.0f)) {
                    m_CameraController->SetMoveSpeed(cameraSpeed);
                }
            }
            ImGui::End();
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
        static RenderPass GetRenderPassEnum(uint32_t index)
        {
            // Map sequential pass index to the RenderPass enum
            // Order: GBuffer, MotionVector, SSAO, Lighting, DeferredLighting, Skybox, Transparent, TAA, PostEffect, Final
            static constexpr std::array<RenderPass, 10> passMap = {
                RenderPass::GBuffer,
                RenderPass::MotionVector,
                RenderPass::SSAO,
                RenderPass::Lighting,
                RenderPass::DeferredLighting,
                RenderPass::Skybox,
                RenderPass::Transparent,
                RenderPass::TAA,
                RenderPass::PostEffect,
                RenderPass::Final
            };
            return index < passMap.size() ? passMap[index] : RenderPass::Final;
        }

    private:
        std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
        std::unique_ptr<CameraController> m_CameraController;
    };
}

#endif // __DEFERRED_RENDER_PIPELINE_H__

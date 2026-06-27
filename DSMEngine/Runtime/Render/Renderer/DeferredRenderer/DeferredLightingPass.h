#pragma once
#ifndef __DEFERRED_LIGHTING_PASS_H__
#define __DEFERRED_LIGHTING_PASS_H__

#include "Runtime/Render/Renderer/CommonPass/RenderResource.h"
#include "Runtime/Render/Renderer/CommonPass/LightingPass.h"
#include "Runtime/Render/Renderer/CommonPass/Shadows.h"
#include "Runtime/Render/ShaderCompiler.h"

namespace DSM {
    class DeferredLightingPass : public IRenderPass
    {
    public:
        DeferredLightingPass(GraphicsRenderer& renderer)
        {
            auto device = renderer.GetDevice();

            m_PassCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(ShaderResource::PassConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("DeferredLightingPassCB"));

            // Binding layout for deferred lighting full-screen pass
            auto bindingLayoutDesc = BindingLayoutDesc{}
                .AddItem(BindingLayoutItem::Texture_SRV(10))      // t10: AlbedoMetallic
                .AddItem(BindingLayoutItem::Texture_SRV(11))      // t11: Normal
                .AddItem(BindingLayoutItem::Texture_SRV(12))      // t12: MaterialAttributes
                .AddItem(BindingLayoutItem::Texture_SRV(13))      // t13: Depth
                .AddItem(BindingLayoutItem::Texture_SRV(14))      // t14: SSAO
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(15)) // t15: TileInfo
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(0)) // b0: PassConstants
                .AddItem(BindingLayoutItem::ConstantBuffer(2))         // b2: LightData (from Light.hlsli)
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(4))   // t4: DirLightData (from Light.hlsli)
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(5))   // t5: OtherLightData (from Light.hlsli)
                .AddItem(BindingLayoutItem::ConstantBuffer(3))         // b3: ShadowConstants (from Shadow.hlsli)
                .AddItem(BindingLayoutItem::Texture_SRV(6))            // t6: DirectionalShadowMap
                .AddItem(BindingLayoutItem::Texture_SRV(7))            // t7: OtherShadowMap
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(8))   // t8: DirShadowMatrices
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(9))   // t9: OtherShadowData
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::AnisoWrap)))
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::PointClamp)))
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::Shadow)));
            m_BindingLayout = device->CreateBindingLayout(bindingLayoutDesc);

            auto createShader = [device](ShaderType type, const auto& entryPoint) {
                ShaderCompileDesc compileDesc = ShaderCompileDesc()
                    .SetType(type)
                    .SetMode(ShaderMode::SM_6_6)
                    .SetFilename("Shaders/DeferredShader/Passes/DeferredLightingPass.hlsl")
                    .SetEnterPoint(entryPoint);
                ShaderByteCode byteCode{compileDesc};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(compileDesc.type)
                    .SetEntryName(compileDesc.enterPoint)
                    .SetDebugName(compileDesc.enterPoint),
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };

            m_VertexShader = createShader(ShaderType::Vertex, "DeferredLightingVS");
            m_PixelShader = createShader(ShaderType::Pixel, "DeferredLightingPS");

            const Viewport& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, viewport.Width(), viewport.Height());
        }

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();

            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("Deferred Lighting Pass Command List"));
            cmdList->Open();

            auto fb = renderRes.GetFramebuffer();
            const auto& fbDesc = fb->GetDesc();
            float width = (float)fb->GetFramebufferInfo().width;
            float height = (float)fb->GetFramebufferInfo().height;

            // Update pass constants
            float cameraNear = renderer.GetCamera().GetNearZ();
            float cameraFar = renderer.GetCamera().GetFarZ();
            ShaderResource::PassConstants passCB{};
            passCB.view = Math::Matrix4::Transpose(renderer.GetCamera().GetViewMatrix());
            passCB.viewInv = Math::Matrix4::Inverse(passCB.view);
            // Apply TAA jitter to projection matrix (must match GBuffer pass)
            auto jitterProj = renderer.GetCamera().GetProjMatrix();
            auto jitterOffset = TaaPass::GetJitterOffset(renderer.GetFrameIndex()) / Math::Vector2{width, height};
            jitterProj.Set(2, 0, jitterProj.Get(2, 0) + jitterOffset.Get(0) * 2.f);
            jitterProj.Set(2, 1, jitterProj.Get(2, 1) + jitterOffset.Get(1) * 2.f);
            passCB.proj = Math::Matrix4::Transpose(jitterProj);
            passCB.projInv = Math::Matrix4::Inverse(passCB.proj);
            passCB.cameraPos = renderer.GetCamera().GetPosition();
            passCB.deltaTime = deltaTime;
            passCB.renderTargetSize = Math::Vector4{width, height, 1.0f / width, 1.0f / height};
            passCB.nearFarZ = Math::Vector4{cameraNear, cameraFar, 1.0f / cameraNear, 1.0f / cameraFar};

            cmdList->WriteBuffer(m_PassCB, &passCB, sizeof(ShaderResource::PassConstants));

            // Rebuild binding set if resources changed
            CheckAndRebind(renderer);

            cmdList->ClearTextureFloat(fbDesc.colorAttachments[0].texture, AllSubresources, Color{0.0f, 0.0f, 0.0f, 1.0f});

            cmdList->SetGraphicsState(GraphicsState{}
                .SetPipeline(m_Pipeline)
                .SetFramebuffer(fb)
                .AddBindingSet(m_BindingSet, 0)
                .SetViewport(ViewportState{}.AddViewportAndScissorRect(renderer.GetCamera().GetViewPort())));

            cmdList->Draw(DrawArguments().SetVertexCount(3));

            cmdList->Close();

            // Wait for lighting pass (tile cull) and SSAO
            device->QueueWaitForCommandList(
                CommandQueueType::Graphics,
                CommandQueueType::Compute,
                RenderResource::GetInstance().GetRenderPassFinishFence(RenderPass::Lighting));

            return device->ExecuteCommandList(cmdList);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            auto& renderRes = RenderResource::GetInstance();
            auto device = renderer.GetDevice();
            auto fb = renderRes.GetFramebuffer();

            m_Pipeline = device->CreateGraphicsPipeline(GraphicsPipelineDesc{}
                .SetVertexShader(m_VertexShader)
                .SetPixelShader(m_PixelShader)
                .AddBindingLayout(m_BindingLayout, 0)
                .SetRenderState(RenderState()
                    .SetDepthStencilState(DepthStencilState()
                        .SetDepthWriteEnable(false)
                        .SetDepthTestEnable(false))
                    .SetRasterState(RasterState().SetCullMode(RasterCullMode::None))),
                fb);

            m_InvalidateCache = true;
        }

    private:
        void CheckAndRebind(GraphicsRenderer& renderer)
        {
            auto& renderRes = RenderResource::GetInstance();

            auto albedoMetallicTex = renderRes.GetCommonTexture(CommonTextureSlot::AlbedoMetallic);
            auto normalTex = renderRes.GetCommonTexture(CommonTextureSlot::Normal);
            auto materialAttribTex = renderRes.GetCommonTexture(CommonTextureSlot::MaterialAttributes);
            auto depthTex = renderRes.GetCommonTexture(CommonTextureSlot::Depth);
            auto ssaoTex = renderRes.GetCommonTexture(CommonTextureSlot::SSAO);
            auto tileInfoBuffer = LightingPass::sm_TileInfoBuffer;
            auto directionalShadowMap = renderRes.GetCommonTexture(CommonTextureSlot::DirectionalShadowMap);
            auto otherShadowMap = renderRes.GetCommonTexture(CommonTextureSlot::OtherShadowMap);

            if (!m_InvalidateCache &&
                m_CacheAlbedoMetallic == albedoMetallicTex &&
                m_CacheNormal == normalTex &&
                m_CacheMaterialAttrib == materialAttribTex &&
                m_CacheDepth == depthTex &&
                m_CacheSSAO == ssaoTex &&
                m_CacheTileInfo == tileInfoBuffer &&
                m_CacheDirectionalShadow == directionalShadowMap &&
                m_CacheOtherShadow == otherShadowMap) {
                return;
            }

            auto& samplers = renderRes;

            m_BindingSet = renderer.GetDevice()->CreateBindingSet(BindingSetDesc{}
                .AddItem(BindingSetItem::Texture_SRV(10, albedoMetallicTex))
                .AddItem(BindingSetItem::Texture_SRV(11, normalTex))
                .AddItem(BindingSetItem::Texture_SRV(12, materialAttribTex))
                .AddItem(BindingSetItem::Texture_SRV(13, depthTex))
                .AddItem(BindingSetItem::Texture_SRV(14, ssaoTex))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(15, tileInfoBuffer))
                .AddItem(BindingSetItem::ConstantBuffer(0, m_PassCB))
                .AddItem(BindingSetItem::ConstantBuffer(2, LightingPass::sm_LightDataBuffer))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(4, LightingPass::sm_DirLightDataBuffer))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(5, LightingPass::sm_OtherLightDataBuffer))
                .AddItem(BindingSetItem::ConstantBuffer(3, Shadows::sm_ShadowCB))
                .AddItem(BindingSetItem::Texture_SRV(6, directionalShadowMap))
                .AddItem(BindingSetItem::Texture_SRV(7, otherShadowMap))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(8, Shadows::sm_DirectionalShadowMatrixBuffer))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(9, Shadows::sm_OtherLightShadowDataBuffer))
                .AddItem(BindingSetItem::Sampler(uint32_t(SamplerSlot::AnisoWrap), renderRes.GetCommonSampler(SamplerSlot::AnisoWrap)))
                .AddItem(BindingSetItem::Sampler(uint32_t(SamplerSlot::PointClamp), renderRes.GetCommonSampler(SamplerSlot::PointClamp)))
                .AddItem(BindingSetItem::Sampler(uint32_t(SamplerSlot::Shadow), renderRes.GetCommonSampler(SamplerSlot::Shadow))),
                m_BindingLayout);

            m_CacheAlbedoMetallic = albedoMetallicTex;
            m_CacheNormal = normalTex;
            m_CacheMaterialAttrib = materialAttribTex;
            m_CacheDepth = depthTex;
            m_CacheSSAO = ssaoTex;
            m_CacheTileInfo = tileInfoBuffer;
            m_CacheDirectionalShadow = directionalShadowMap;
            m_CacheOtherShadow = otherShadowMap;
            m_InvalidateCache = false;
        }

        BufferHandle m_PassCB{};
        ShaderHandle m_VertexShader{};
        ShaderHandle m_PixelShader{};
        GraphicsPipelineHandle m_Pipeline{};
        BindingLayoutHandle m_BindingLayout{};
        BindingSetHandle m_BindingSet{};

        // Cache for rebinding
        ITexture* m_CacheAlbedoMetallic = nullptr;
        ITexture* m_CacheNormal = nullptr;
        ITexture* m_CacheMaterialAttrib = nullptr;
        ITexture* m_CacheDepth = nullptr;
        ITexture* m_CacheSSAO = nullptr;
        IBuffer* m_CacheTileInfo = nullptr;
        ITexture* m_CacheDirectionalShadow = nullptr;
        ITexture* m_CacheOtherShadow = nullptr;
        bool m_InvalidateCache = true;
    };
}

#endif // __DEFERRED_LIGHTING_PASS_H__
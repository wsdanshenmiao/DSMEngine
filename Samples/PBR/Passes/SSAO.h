#pragma once
#ifndef __SSAO_H__
#define __SSAO_H__

#include "IRenderPass.h"

namespace DSM {

    class SSAO : public IRenderPass 
    {
    public:
        SSAO(Renderer& renderer)
            : m_SSAOTex(g_RenderResources.commonTextures[(size_t)CommonTextureSlot::SSAO]) 
        {
            auto device = renderer.GetDevice();

            auto fbDesc = g_RenderResources.framebuffer->GetFramebufferInfo();

            m_SSAOConstants = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(Math::Matrix4))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("SSAO Constants"));

            m_BindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .SetVisibility(ShaderType::Compute)
                .AddItem(BindingLayoutItem::Texture_UAV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(1))
                .AddItem(BindingLayoutItem::Texture_SRV(2))
                .AddItem(BindingLayoutItem::Sampler(0))
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(0)));

            ShaderByteCode csByteCode{ShaderCompileDesc()
                .SetType(ShaderType::Compute)
                .SetMode(ShaderMode::SM_6_6)
                .SetEnterPoint("SSAOCS")
                .SetFilename("Shaders/Passes/SSAO.hlsl")};
            auto cs = device->CreateShader(ShaderDesc()
                .SetShaderType(ShaderType::Compute)
                .SetEntryName("SSAOCS")
                .SetDebugName("SSAO Compute Shader"),
                csByteCode.GetByteCode(), csByteCode.GetByteCodeSize());
            m_ComputePipeline = device->CreateComputePipeline(ComputePipelineDesc()
                .SetComputeShader(cs)
                .AddBindingLayout(m_BindingLayout, 0));

            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Common]
                .AddItem(BindingLayoutItem::Texture_SRV(8));

            OnResize(renderer, fbDesc.width, fbDesc.height);
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            auto cmdList = renderer.GetDevice()->CreateCommandList(CommandListParameters()
                .SetDebugName("SSAO Command List")
                .SetQueueType(CommandQueueType::Compute));

            cmdList->Open();
            
            static bool preEnable = sm_Enable;
            if(sm_Enable){
                SSAOConstants ssaoConstants{};
                ssaoConstants.proj = Math::Matrix4::Transpose(renderer.GetCamera().GetProjMatrix());
                ssaoConstants.projInv = Math::Matrix4::Inverse(ssaoConstants.proj);
                ssaoConstants.sampleCount = std::min(sm_SampleCount, 14u);
                ssaoConstants.occlusionRadius = sm_OcclusionRadius;
                ssaoConstants.ssaoThreshold = sm_OcclusionThreshold;
                cmdList->WriteBuffer(m_SSAOConstants, &ssaoConstants, sizeof(ssaoConstants));

                cmdList->SetComputeState(ComputeState()
                    .SetPipeline(m_ComputePipeline)
                    .AddBindingSet(m_BindingSet));
                cmdList->Dispatch(m_SSAOTex->GetDesc().width, m_SSAOTex->GetDesc().height, 1);
            }
            else if(preEnable){
                cmdList->ClearTextureFloat(m_SSAOTex, AllSubresources, Color{1,1,1,1});
            }
            preEnable = sm_Enable;
            
            cmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(cmdList);
        }
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            auto device = renderer.GetDevice();

            // 需要先移除先前的绑定
            std::erase(g_RenderResources.commonBindingSetDesc.bindings, BindingSetItem::Texture_SRV(8, m_SSAOTex));

            m_SSAOTex = device->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::R32_FLOAT)
                .SetIsUAV(true)
                .SetInitialState(ResourceStates::UnorderedAccess)
                .SetKeepInitialState(true)
                .SetDebugName("SSAO Texture"));

            g_RenderResources.commonBindingSetDesc
                .AddItem(BindingSetItem::Texture_SRV(8, m_SSAOTex));

            auto& depthTex = g_RenderResources.framebuffer->GetDesc().depthAttachment.texture;
            auto& normalTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Normal];
            auto& noiseTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Noise];
            m_BindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::Texture_UAV(0, m_SSAOTex))
                .AddItem(BindingSetItem::Texture_SRV(0, normalTex))
                .AddItem(BindingSetItem::Texture_SRV(1, depthTex))
                .AddItem(BindingSetItem::Texture_SRV(2, noiseTex))
                .AddItem(BindingSetItem::Sampler(0, GetCommonSampler(SamplerSlot::AnisoWrap)))
                .AddItem(BindingSetItem::ConstantBuffer(0, m_SSAOConstants)),
                m_BindingLayout);
        }
    
    public:
        inline static bool sm_Enable = true;
        inline static uint32_t sm_SampleCount = 14;
        inline static float sm_OcclusionRadius = 0.4f;
        inline static float sm_OcclusionThreshold = 0.05f;

    private:
        TextureHandle& m_SSAOTex;
        BufferHandle m_SSAOConstants;

        BindingLayoutHandle m_BindingLayout;
        ComputePipelineHandle m_ComputePipeline;
        BindingSetHandle m_BindingSet;
    };

} // namespace DSM


#endif // !__SSAO_H__
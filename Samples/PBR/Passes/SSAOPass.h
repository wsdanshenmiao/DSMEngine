#pragma once
#ifndef __SSAOPASS_H__
#define __SSAOPASS_H__

#include "IRenderPass.h"

namespace DSM {
    struct SSAOSettings
    {
        uint32_t sampleCount = 14;
        float occlusionRadius = 0.4f;
        float occlusionThreshold = 0.05f;
        float fadeEnd = 2; // SSAO 衰减结束距离
        uint32_t contrast = 2; // SSAO 的对比度
        uint32_t blurRadius = 5; // 模糊半径
        uint32_t blurCount = 1; // 模糊次数
        bool enable = true;
    };

    class SSAOPass : public IRenderPass 
    {
    public:
        SSAOPass(Renderer& renderer)
            : m_SSAOTex(g_RenderResources.commonTextures[(size_t)CommonTextureSlot::SSAO]) 
        {
            auto device = renderer.GetDevice();

            auto fbDesc = g_RenderResources.framebuffer->GetFramebufferInfo();

            sm_TimerQuery = device->CreateTimerQuery();

            m_SSAOConstants = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(SSAOConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("SSAO Constants"));

            m_BlurConstants = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(SSAOBlurConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("SSAO Blur Constants"));

            m_BlurWeights = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(float) * (sm_MaxBlurRadius * 2 + 1))
                .SetStructStride(sizeof(float))
                .SetDebugName("SSAO Blur Weights"));

            auto bindingLayoutDesc = BindingLayoutDesc()
                .SetVisibility(ShaderType::Compute)
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem::Texture_UAV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(1))
                .AddItem(BindingLayoutItem::Texture_SRV(2));

            // SSAO 的绑定布局
            m_SSAOBindingLayout = device->CreateBindingLayout(BindingLayoutDesc(bindingLayoutDesc)
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::PointWrap)))  // 采样噪声
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::LinearBorder)))); // 采样深度及法线

            // 对 SSAO 进行滤波的绑定布局
            m_BlurBindingLayout = device->CreateBindingLayout(BindingLayoutDesc(bindingLayoutDesc)
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(3))
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::PointBorder)))
            );


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
            m_SSAOPipeline = device->CreateComputePipeline(ComputePipelineDesc()
                .SetComputeShader(cs)
                .AddBindingLayout(m_SSAOBindingLayout, 0));

            ShaderByteCode blurCsByteCode{ShaderCompileDesc()
                .SetType(ShaderType::Compute)
                .SetMode(ShaderMode::SM_6_6)
                .SetEnterPoint("SSAOBlurCS")
                .SetFilename("Shaders/Passes/SSAOBlur.hlsl")};
            auto blurCs = device->CreateShader(ShaderDesc()
                .SetShaderType(ShaderType::Compute)
                .SetEntryName("SSAOBlurCS")
                .SetDebugName("SSAO Blur Compute Shader"),
                blurCsByteCode.GetByteCode(), blurCsByteCode.GetByteCodeSize());
            m_BlurPipeline = device->CreateComputePipeline(ComputePipelineDesc()
                .SetComputeShader(blurCs)
                .AddBindingLayout(m_BlurBindingLayout, 0));

            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Common]
                .AddItem(BindingLayoutItem::Texture_SRV(LitPassBindingLayout::ShaderResource::SSAO));

            OnResize(renderer, fbDesc.width, fbDesc.height);
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            auto cmdList = renderer.GetDevice()->CreateCommandList(CommandListParameters()
                .SetDebugName("SSAO Command List")
                .SetQueueType(CommandQueueType::Compute));

            cmdList->Open();

            cmdList->BeginTimerQuery(sm_TimerQuery);

            static bool preEnable = sm_Settings.enable;
            if(sm_Settings.enable){
                SSAOConstants ssaoConstants{};
                ssaoConstants.proj = Math::Matrix4::Transpose(renderer.GetCamera().GetProjMatrix());
                ssaoConstants.projInv = Math::Matrix4::Inverse(ssaoConstants.proj);
                ssaoConstants.sampleCount = std::min(sm_Settings.sampleCount, 14u);
                ssaoConstants.occlusionRadius = sm_Settings.occlusionRadius;
                ssaoConstants.ssaoThreshold = sm_Settings.occlusionThreshold;
                ssaoConstants.fadeEnd = sm_Settings.fadeEnd;
                ssaoConstants.contrast = sm_Settings.contrast;
                cmdList->WriteBuffer(m_SSAOConstants, &ssaoConstants, sizeof(ssaoConstants));

                cmdList->SetTextureState(m_SSAOTex, AllSubresources, ResourceStates::UnorderedAccess);
                cmdList->SetComputeState(ComputeState()
                    .SetPipeline(m_SSAOPipeline)
                    .AddBindingSet(m_SSAOBindingSet));
                auto groupX = Math::DivideByMultiple(m_SSAOTex->GetDesc().width, 16u);
                auto groupY = Math::DivideByMultiple(m_SSAOTex->GetDesc().height, 16u);
                cmdList->Dispatch(groupX, groupY, 1);

                BlurSSAO(renderer, cmdList);
            }
            else if(preEnable){
                cmdList->ClearTextureFloat(m_SSAOTex, AllSubresources, Color{1,1,1,1});
            }
            preEnable = sm_Settings.enable;

            cmdList->EndTimerQuery(sm_TimerQuery);

            cmdList->Close();
            renderer.GetDevice()->QueueWaitForCommandList(CommandQueueType::Compute, CommandQueueType::Graphics, GeometryPass::sm_LastFrameTime);
            sm_LastFrameTime = renderer.GetDevice()->ExecuteCommandList(cmdList);
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            auto device = renderer.GetDevice();

            // 需要先移除先前的绑定
            auto binding = BindingSetItem::Texture_SRV(LitPassBindingLayout::ShaderResource::SSAO, m_SSAOTex);
            std::erase(g_RenderResources.commonBindingSetDesc.bindings, binding);

            // 降分辨率生成
            auto texDesc = TextureDesc()
                .SetWidth(std::max(width / 2, 1u))
                .SetHeight(std::max(height / 2, 1u))
                .SetFormat(Format::R32_FLOAT)
                .SetIsUAV(true)
                .SetInitialState(ResourceStates::UnorderedAccess)
                .SetKeepInitialState(true)
                .SetDebugName("SSAO Texture");
            m_SSAOTex = device->CreateTexture(texDesc);
            m_TmpTexture = device->CreateTexture(texDesc);

            g_RenderResources.commonBindingSetDesc
                .AddItem(BindingSetItem::Texture_SRV(LitPassBindingLayout::ShaderResource::SSAO, m_SSAOTex));

            auto& depthTex = g_RenderResources.framebuffer->GetDesc().depthAttachment.texture;
            auto& normalTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Normal];
            auto& noiseTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Noise];
            m_SSAOBindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::Texture_UAV(0, m_SSAOTex))
                .AddItem(BindingSetItem::Texture_SRV(0, normalTex))
                .AddItem(BindingSetItem::Texture_SRV(1, depthTex))
                .AddItem(BindingSetItem::Texture_SRV(2, noiseTex))
                .AddItem(BindingSetItem::Sampler(size_t(SamplerSlot::PointWrap), GetCommonSampler(SamplerSlot::PointWrap)))
                .AddItem(BindingSetItem::Sampler(size_t(SamplerSlot::LinearBorder), GetCommonSampler(SamplerSlot::LinearBorder)))
                .AddItem(BindingSetItem::ConstantBuffer(0, m_SSAOConstants)),
                m_SSAOBindingLayout);

            auto blurBindingSetDesc = BindingSetDesc()
                .AddItem(BindingSetItem::ConstantBuffer(0, m_BlurConstants))
                .AddItem(BindingSetItem::Texture_SRV(1, normalTex))
                .AddItem(BindingSetItem::Texture_SRV(2, depthTex))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(3, m_BlurWeights))
                .AddItem(BindingSetItem::Sampler(size_t(SamplerSlot::PointBorder), GetCommonSampler(SamplerSlot::PointBorder)));
            m_BlurToSSAOBindingSet = device->CreateBindingSet(BindingSetDesc(blurBindingSetDesc)
                .AddItem(BindingSetItem::Texture_UAV(0, m_SSAOTex))
                .AddItem(BindingSetItem::Texture_SRV(0, m_TmpTexture)),
                m_BlurBindingLayout);
            m_BlurToTmpBindingSet = device->CreateBindingSet(BindingSetDesc(blurBindingSetDesc)
                .AddItem(BindingSetItem::Texture_UAV(0, m_TmpTexture))
                .AddItem(BindingSetItem::Texture_SRV(0, m_SSAOTex)),
                m_BlurBindingLayout);
        }
    
    private:
        void BlurSSAO(Renderer& renderer, ICommandList* cmdList)
        {
            if(sm_Settings.blurRadius < 1) 
                return;

            static uint32_t preBlurRadius = 0;
            if(sm_Settings.blurRadius != preBlurRadius){
                sm_Settings.blurRadius = std::min(sm_Settings.blurRadius, sm_MaxBlurRadius);
                std::vector<float> weights = CalculGaussWeights(sm_Settings.blurRadius);
                cmdList->WriteBuffer(m_BlurWeights, weights.data(), sizeof(float) * weights.size());
            }

            SSAOBlurConstants blurConstants{};
            blurConstants.proj = Math::Matrix4::Transpose(renderer.GetCamera().GetProjMatrix());
            blurConstants.blurRadius = sm_Settings.blurRadius;
            for(int i = 0; i < sm_Settings.blurCount; ++i){
                blurConstants.isHorizontal = true;
                cmdList->WriteBuffer(m_BlurConstants, &blurConstants, sizeof(blurConstants));
                
                cmdList->SetTextureState(m_SSAOTex, AllSubresources, ResourceStates::NoPixelShaderResource);
                cmdList->SetTextureState(m_TmpTexture, AllSubresources, ResourceStates::UnorderedAccess);
                cmdList->SetComputeState(ComputeState()
                    .SetPipeline(m_BlurPipeline)
                    .AddBindingSet(m_BlurToTmpBindingSet));
                uint32_t groupX = Math::Align(m_TmpTexture->GetDesc().width, sm_ThreadSize) / sm_ThreadSize;
                cmdList->Dispatch(groupX, m_TmpTexture->GetDesc().height, 1);

                blurConstants.isHorizontal = false;
                cmdList->WriteBuffer(m_BlurConstants, &blurConstants, sizeof(blurConstants));
                cmdList->SetTextureState(m_TmpTexture, AllSubresources, ResourceStates::NoPixelShaderResource);
                cmdList->SetTextureState(m_SSAOTex, AllSubresources, ResourceStates::UnorderedAccess);
                cmdList->SetComputeState(ComputeState()
                    .SetPipeline(m_BlurPipeline)
                    .AddBindingSet(m_BlurToSSAOBindingSet));
                groupX = Math::Align(m_TmpTexture->GetDesc().height, sm_ThreadSize) / sm_ThreadSize;
                cmdList->Dispatch(groupX, m_SSAOTex->GetDesc().width, 1);
            }
        }

        float GaussFunction(float x, float sigma) const
        {
            return std::exp(- x * x / (2 * sigma * sigma));
        }

        std::vector<float> CalculGaussWeights(int blurRadius) const
        {
            // 取 2 sigma 范围，计算高斯核范围
            float sigma = (float)blurRadius / 2.0f;

            // 计算权重
            float weightSum = 0;
            std::vector<float> ret(2 * blurRadius + 1);
            for (int i = -int(blurRadius); i <= blurRadius; ++i) {
                ret[i + blurRadius] = GaussFunction(i, sigma);
                weightSum += ret[i + blurRadius];
            }
            
            float invWeightSum = 1 / weightSum;
            for (auto& weight : ret) {
                weight *= invWeightSum;
            }

            return ret;
        }


    public:
        inline static constexpr uint32_t sm_MaxBlurRadius = 5;
        inline static uint64_t sm_LastFrameTime = 0;
        inline static SSAOSettings sm_Settings{};
        inline static TimerQueryHandle sm_TimerQuery{};

    private:
        inline static constexpr uint32_t sm_ThreadSize = 256;

        TextureHandle& m_SSAOTex;
        TextureHandle m_TmpTexture{};

        BufferHandle m_SSAOConstants;
        BufferHandle m_BlurConstants;
        BufferHandle m_BlurWeights;

        BindingLayoutHandle m_SSAOBindingLayout;
        BindingLayoutHandle m_BlurBindingLayout;

        BindingSetHandle m_SSAOBindingSet;
        BindingSetHandle m_BlurToSSAOBindingSet;
        BindingSetHandle m_BlurToTmpBindingSet;

        ComputePipelineHandle m_SSAOPipeline;
        ComputePipelineHandle m_BlurPipeline;
    };

} // namespace DSM


#endif // !__SSAOPASS_H__
#pragma once
#ifndef __BLOOMPASS_H__
#define __BLOOMPASS_H__

#include "PostEffectManager.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Math/MathCommon.h"
#include "Runtime/Render/Renderer/ForwardRenderer/GaussianBlurPass.h"

#include <algorithm>
#include <vector>

namespace DSM{
    struct BloomSettings
    {
        float threshold = 0.8f;
        uint32_t blurRadius = 3;
        uint32_t blurCount = 3;
    };


    class BloomPass : public IPostEffect
    {
    public:
        BloomPass(Renderer& renderer)
            : m_GaussianBlurPass(std::make_unique<GaussianBlurPass>(renderer))
        {
            auto device = renderer.GetDevice();
            auto bindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .SetVisibility(ShaderType::Compute)
                .AddItem(BindingLayoutItem::PushConstants(0, sizeof(float)))
                .AddItem(BindingLayoutItem::Texture_UAV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(0)));
            
            auto createShader = [device](const auto& enter, const auto& name){
                auto extractCSByteCode = ShaderByteCode{ShaderCompileDesc{}
                    .SetType(ShaderType::Compute)
                    .SetMode(ShaderMode::SM_6_6)
                    .SetFilename("Shaders/ForwardShader/PostEffect/Bloom.hlsl")
                    .SetEnterPoint(enter)};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(ShaderType::Compute)
                    .SetDebugName(name)
                    .SetEntryName(enter), 
                    extractCSByteCode.GetByteCode(), extractCSByteCode.GetByteCodeSize());
            };
        
            m_ExtractPipeline = device->CreateComputePipeline(ComputePipelineDesc{}
                .AddBindingLayout(bindingLayout, 0)
                .SetComputeShader(createShader("BloomExtractCS", "Bloom Extract Compute Shader")));
            m_CompositePipeline = device->CreateComputePipeline(ComputePipelineDesc{}
                .AddBindingLayout(bindingLayout, 0)
                .SetComputeShader(createShader("BloomCompositeCS", "Bloom Composite Compute Shader")));
        }

        void SetSettings(const BloomSettings& settings) { m_Settings = settings; }

        void Render(Renderer& renderer, ICommandList* cmdList, float deltaTime, ITexture* srcTex, ITexture* dstTex) override
        {
            auto device = renderer.GetDevice();
            if(srcTex != m_CacheSrcTex || dstTex != m_CacheDstTex){
                m_CacheSrcTex = srcTex;
                m_CacheDstTex = dstTex;
                m_BindingSet = device->CreateBindingSet(BindingSetDesc()
                    .AddItem(BindingSetItem::PushConstants(0, sizeof(float)))
                    .AddItem(BindingSetItem::Texture_UAV(0, dstTex))
                    .AddItem(BindingSetItem::Texture_SRV(0, srcTex)),
                    m_ExtractPipeline->GetDesc().bindingLayouts[0]);
            }

            cmdList->Open();

            cmdList->SetTextureState(srcTex, AllSubresources, ResourceStates::NoPixelShaderResource);
            cmdList->SetTextureState(dstTex, AllSubresources, ResourceStates::UnorderedAccess);
            
            cmdList->SetComputeState(ComputeState()
                .SetPipeline(m_ExtractPipeline)
                .AddBindingSet(m_BindingSet));
            cmdList->SetPushConstants(&m_Settings.threshold, sizeof(float));
            uint32_t groupX = Math::DivideByMultiple(srcTex->GetDesc().width, sm_ThreadSize);
            uint32_t groupY = Math::DivideByMultiple(srcTex->GetDesc().height, sm_ThreadSize);
            cmdList->Dispatch(groupX, groupY);

            GaussianBlurSettings blurSettings{m_Settings.blurRadius, m_Settings.blurCount};
            m_GaussianBlurPass->BlurTexture(renderer, cmdList, dstTex, blurSettings);

            cmdList->SetTextureState(dstTex, AllSubresources, ResourceStates::UnorderedAccess);

            cmdList->SetComputeState(ComputeState()
                .SetPipeline(m_CompositePipeline)
                .AddBindingSet(m_BindingSet));
            cmdList->Dispatch(groupX, groupY);

            cmdList->Close();
            device->ExecuteCommandList(cmdList);
        }

    private:
        static constexpr uint32_t sm_MaxBlurRadius = 15;
        static constexpr uint32_t sm_ThreadSize = 8;

        BloomSettings m_Settings{};
        
        ComputePipelineHandle m_ExtractPipeline{};
        ComputePipelineHandle m_CompositePipeline{};
        
        ITexture* m_CacheSrcTex = nullptr;
        ITexture* m_CacheDstTex = nullptr;
        BindingSetHandle m_BindingSet{};
        
        std::unique_ptr<GaussianBlurPass> m_GaussianBlurPass{};
    };
}


#endif // __BLOOMPASS_H__
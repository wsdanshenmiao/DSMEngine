#pragma once
#ifndef __SSAO_H__
#define __SSAO_H__

#include "IRenderPass.h"
#include "Runtime/Render/ShaderCompiler.h"

namespace DSM {

    class SSAO : public IRenderPass 
    {
    public:
        SSAO(Renderer& renderer)
            : m_SSAOTex(g_RenderResources.commonTextures[(size_t)CommonTextureSlot::SSAO]) 
        {
            auto device = renderer.GetDevice();

            auto fbDesc = g_RenderResources.framebuffer->GetFramebufferInfo();
        
            auto bindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .SetVisibility(ShaderType::Compute)
                .AddItem(BindingLayoutItem::Texture_UAV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(1))
                .AddItem(BindingLayoutItem::Texture_SRV(2)));

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
                .AddBindingLayout(bindingLayout, 0));

            OnResize(renderer, fbDesc.width, fbDesc.height);
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            auto cmdList = renderer.GetDevice()->CreateCommandList(CommandListParameters()
                .SetDebugName("SSAO Command List")
                .SetQueueType(CommandQueueType::Compute));

            cmdList->Open();

            cmdList->SetComputeState(ComputeState()
                .SetPipeline(m_ComputePipeline)
                .AddBindingSet(m_BindingSet));
            cmdList->Dispatch(m_SSAOTex->GetDesc().width, m_SSAOTex->GetDesc().height, 1);
            
            cmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(cmdList);
        }
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            auto device = renderer.GetDevice();

            m_SSAOTex = device->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::R8_UNORM)
                .SetIsUAV(true)
                .SetClearValue(Color(1.0f))
                .SetInitialState(ResourceStates::UnorderedAccess)
                .SetDebugName("SSAO Texture"));

            auto& depthTex = g_RenderResources.framebuffer->GetDesc().depthAttachment.texture;
            auto& normalTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Normal];
            auto& noiseTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Noise];
            m_BindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::Texture_UAV(0, m_SSAOTex))
                .AddItem(BindingSetItem::Texture_SRV(0, normalTex))
                .AddItem(BindingSetItem::Texture_SRV(1, depthTex))
                .AddItem(BindingSetItem::Texture_SRV(2, noiseTex)),
                m_BindingLayout);
        }
    
    private:
        TextureHandle& m_SSAOTex;

        BindingLayoutHandle m_BindingLayout;
        ComputePipelineHandle m_ComputePipeline;
        BindingSetHandle m_BindingSet;
    };

} // namespace DSM


#endif // !__SSAO_H__
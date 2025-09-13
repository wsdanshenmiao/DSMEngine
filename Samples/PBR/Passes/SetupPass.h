#pragma once
#ifndef __SETUPPASS_H__
#define __SETUPPASS_H__

#include "IRenderPass.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Shaders/ResourceData.h"

namespace DSM {
    class SetupPass : public IRenderPass
    {
    public:
        SetupPass(Renderer& renderer)
        {
            auto device = renderer.GetDevice();

            // 创建着色器
            ShaderCompileDesc vsDesc{};
            vsDesc.SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_1)
                .SetFilename("Shaders/Lit.hlsl")
                .SetEnterPoint("LitPassVS");
            ShaderByteCode vsNoTangent{vsDesc};
            ShaderByteCode vs{vsDesc.AddDefine("USE_TANGENT", "1")};

            ShaderCompileDesc psDesc{};
            psDesc.SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_1)
                .SetFilename("Shaders/Lit.hlsl")
                .SetEnterPoint("LitPassPS");
            ShaderByteCode psNoTangent{psDesc};
            ShaderByteCode ps{psDesc.AddDefine("USE_TANGENT", "1")};

            auto& shaders = g_RenderResources.shaders;
            shaders[(size_t)ShaderSlot::LitVS] = device->CreateShader(ShaderDesc()
                .SetEntryName(vs.GetDesc().m_EnterPoint)
                .SetShaderType(vs.GetDesc().m_Type)
                .SetDebugName("LitPassVS"), 
                vs.GetByteCode(), vs.GetByteCodeSize());
            shaders[(size_t)ShaderSlot::LitVSNoTangent] = device->CreateShader(ShaderDesc()
                .SetEntryName(vsNoTangent.GetDesc().m_EnterPoint)
                .SetShaderType(vsNoTangent.GetDesc().m_Type)
                .SetDebugName("LitPassVSNoTangent"), 
                vsNoTangent.GetByteCode(), vsNoTangent.GetByteCodeSize());
            shaders[(size_t)ShaderSlot::LitPS] = device->CreateShader(ShaderDesc()
                .SetEntryName(ps.GetDesc().m_EnterPoint)
                .SetShaderType(ps.GetDesc().m_Type)
                .SetDebugName("LitPassPS"), 
                ps.GetByteCode(), ps.GetByteCodeSize());
            shaders[(size_t)ShaderSlot::LitPSNoTangent] = device->CreateShader(ShaderDesc()
                .SetEntryName(psNoTangent.GetDesc().m_EnterPoint)
                .SetShaderType(psNoTangent.GetDesc().m_Type)
                .SetDebugName("LitPassPSNoTangent"), 
                psNoTangent.GetByteCode(), psNoTangent.GetByteCodeSize());

            m_Sampler = renderer.GetDevice()->CreateSampler(SamplerDesc().SetAllAddressModes(SamplerAddressMode::Wrap));
        

            g_RenderResources.bindingLayoutDesc
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(0)) // MeshConstants
                .AddItem(BindingLayoutItem().ConstantBuffer(1)) // MaterialData
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(2)) // PassConstants
                .AddItem(BindingLayoutItem().SetType(ResourceType::Texture_SRV).SetSlot(0).SetSize(kNumTextures)) // 10 个用于 PBR 的纹理
                .AddItem(BindingLayoutItem().Sampler(0));   // 默认采样器

            g_RenderResources.bindingSetDesc
                .AddItem(BindingSetItem().Sampler(0, m_Sampler));

            const auto& bufferDesc = renderer.GetCurrentBackBuffer()->GetDesc();
            OnResize(renderer, bufferDesc.width, bufferDesc.height);
        }

        void Render(DSM::Renderer& renderer, float deltaTime) override {}
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            m_ColorTex = renderer.GetDevice()->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(renderer.GetCurrentBackBuffer()->GetDesc().format)
                .SetClearValue(Color{1, 0.7f, 0.75f, 1})
                .SetInitialState(ResourceStates::RenderTarget)
                .SetIsRenderTarget(true)
                .SetDebugName("ColorTex"));
            m_DepthTex = renderer.GetDevice()->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::D24S8)
                .SetClearValue(Color{1, 0, 0, 0})
                .SetInitialState(ResourceStates::DepthWrite)
                .SetIsRenderTarget(true)    // 深度纹理也需要设置
                .SetDebugName("DepthTex"));
            g_RenderResources.framebuffer = renderer.GetDevice()->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(m_ColorTex).SetDepthAttachment(m_DepthTex));

            for(auto& [desc, pipeline] : g_RenderResources.psoCache){
                pipeline = renderer.GetDevice()->CreateGraphicsPipeline(desc, g_RenderResources.framebuffer);
            }
        }

    private:
        TextureHandle m_ColorTex;
        TextureHandle m_DepthTex;

        SamplerHandle m_Sampler;
    };

} // namespace DSM


#endif
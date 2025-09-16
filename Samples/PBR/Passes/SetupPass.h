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
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/LitPass.hlsl")
                .SetEnterPoint("LitPassVS");
            ShaderByteCode vsNoTangent{vsDesc};
            ShaderByteCode vs{vsDesc.AddDefine("USE_TANGENT", "1")};

            ShaderCompileDesc psDesc{};
            psDesc.SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/LitPass.hlsl")
                .SetEnterPoint("LitPassPS");
            ShaderByteCode psNoTangent{psDesc};
            ShaderByteCode ps{psDesc.AddDefine("USE_TANGENT", "1")};

            auto createShader = [&](const ShaderByteCode& byteCode, const auto& name) {
                return device->CreateShader(ShaderDesc()
                    .SetEntryName(byteCode.GetDesc().enterPoint)
                    .SetShaderType(byteCode.GetDesc().type)
                    .SetDebugName(name), 
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };
            auto& shaders = g_RenderResources.shaders;
            shaders[(size_t)ShaderSlot::LitVS] = createShader(vs, "LitPassVS");
            shaders[(size_t)ShaderSlot::LitVSNoTangent] = createShader(vsNoTangent, "LitPassVSNoTangent");
            shaders[(size_t)ShaderSlot::LitPS] = createShader(ps, "LitPassPS");
            shaders[(size_t)ShaderSlot::LitPSNoTangent] = createShader(psNoTangent, "LitPassPSNoTangent");

            sm_Sampler = renderer.GetDevice()->CreateSampler(SamplerDesc().SetAllAddressModes(SamplerAddressMode::Wrap));
        

            g_RenderResources.bindingLayoutDesc
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(0)) // MeshConstants
                .AddItem(BindingLayoutItem().ConstantBuffer(1)) // MaterialData
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(2)) // PassConstants
                .AddItem(BindingLayoutItem().SetType(ResourceType::Texture_SRV).SetSlot(0).SetSize(kNumTextures)) // 10 个用于 PBR 的纹理
                .AddItem(BindingLayoutItem().Sampler(0));   // 默认采样器

            g_RenderResources.bindingSetDesc
                .AddItem(BindingSetItem().Sampler(0, sm_Sampler));

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

    public:
        inline static SamplerHandle sm_Sampler{};

    private:
        TextureHandle m_ColorTex;
        TextureHandle m_DepthTex;
    };

} // namespace DSM


#endif
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

            bool reverseZ = renderer.GetCamera().IsReversedZ();
            sm_Sampler = renderer.GetDevice()->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Wrap)
                .SetComparisonFunc(reverseZ ? ComparisonFunc::Greater : ComparisonFunc::Less));
            
            //CreateShader(renderer);

            g_RenderResources.bindingLayoutDesc
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(0)) // MeshConstants
                .AddItem(BindingLayoutItem().ConstantBuffer(1)) // MaterialData
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(2)) // PassConstants
                .AddItem(BindingLayoutItem().SetType(ResourceType::Texture_SRV).SetSlot(0).SetSize(kNumTextures)); // 10 个用于 PBR 的纹理
            
            g_RenderResources.samplerBindingLayoutDesc.AddItem(BindingLayoutItem().Sampler(0));   // 默认采样器
            g_RenderResources.samplerBindingSetDesc.AddItem(BindingSetItem().Sampler(0, sm_Sampler));

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

        void CreateShader(Renderer& renderer)
        {
            auto device = renderer.GetDevice();
            
            // 创建着色器
            ShaderCompileDesc litVSDesc{};
            litVSDesc.SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/LitPass.hlsl")
                .SetEnterPoint("LitPassVS");
            ShaderByteCode litVSNoTangent{litVSDesc};
            ShaderByteCode litVS{litVSDesc.AddDefine("USE_TANGENT", "1")};

            ShaderCompileDesc litPSDesc{};
            litPSDesc.SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/LitPass.hlsl")
                .SetEnterPoint("LitPassPS");
            ShaderByteCode litPSNoTangent{litPSDesc};
            ShaderByteCode litPS{litPSDesc.AddDefine("USE_TANGENT", "1")};


            // 编译 ShadowPass 的着色器
            auto shadowVSDesc = ShaderCompileDesc()
                .SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_6)
                .SetEnterPoint("ShadowPassVS")
                .SetFilename("Shaders/ShadowPass.hlsl");
            ShaderByteCode shadowVS{shadowVSDesc};
            shadowVSDesc.AddDefine("SHADOWS_CLIP", "1");
            ShaderByteCode shadowVSClip{shadowVSDesc};
            auto shadowPSDesc = ShaderCompileDesc()
                .SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_6)
                .SetEnterPoint("ShadowPassPS")
                .SetFilename("Shaders/ShadowPass.hlsl");
            ShaderByteCode shadowPS{shadowPSDesc};
            shadowPSDesc.AddDefine("SHADOWS_CLIP", "1");
            ShaderByteCode shadowPSClip{shadowPSDesc};

            if(!litVSNoTangent.IsValid() ||  !litVS.IsValid() ||
                !litPSNoTangent.IsValid() || !litPS.IsValid() ||
                !shadowVS.IsValid() || !shadowVSClip.IsValid() ||
                !shadowPS.IsValid() || !shadowPSClip.IsValid()) {
                return;
            }

            auto createShader = [&](const ShaderByteCode& byteCode, const auto& name) {
                return device->CreateShader(ShaderDesc()
                    .SetEntryName(byteCode.GetDesc().enterPoint)
                    .SetShaderType(byteCode.GetDesc().type)
                    .SetDebugName(name), 
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };
            auto& shaders = g_RenderResources.shaders;
            shaders[(size_t)ShaderSlot::LitVS] = createShader(litVS, "LitPassVS");
            shaders[(size_t)ShaderSlot::LitVSNoTangent] = createShader(litVSNoTangent, "LitPassVSNoTangent");
            shaders[(size_t)ShaderSlot::LitPS] = createShader(litPS, "LitPassPS");
            shaders[(size_t)ShaderSlot::LitPSNoTangent] = createShader(litPSNoTangent, "LitPassPSNoTangent");

            shaders[size_t(ShaderSlot::ShadowVS)] = createShader(shadowVS, "ShadowPassVS");
            shaders[size_t(ShaderSlot::ShadowVSClip)] = createShader(shadowVSClip, "ShadowPassVSClip");
            shaders[size_t(ShaderSlot::ShadowPS)] = createShader(shadowPS, "ShadowPassPS");
            shaders[size_t(ShaderSlot::ShadowPSClip)] = createShader(shadowPSClip, "ShadowPassPSClip");

            auto findShader = [](const std::vector<ShaderHandle>& shaders, const ShaderDesc& desc){
                return std::ranges::find_if(shaders, [&desc](const ShaderHandle& shader) {
                    return shader->GetDesc() == desc;
                });
            };

            auto updateConfig = [&](std::vector<RenderConfig>& configs, const auto& preDesc, const auto& newDesc) {
                auto it = std::ranges::find_if(configs, [&preDesc](const RenderConfig& config) {
                    return config.pipelineDesc == preDesc;
                });
                if (it != configs.end()) {
                    it->pipelineDesc = newDesc;
                }
            };

            std::vector<GraphicsPipelineHandle> pipelines;
            for (auto& [desc, pipeline] : g_RenderResources.psoCache) {
                auto newDesc = desc;
                auto shader = findShader(shaders, desc.VS->GetDesc());
                if(shader == shaders.end())
                    continue;
                newDesc.VS = *shader;
                shader = findShader(shaders, desc.PS->GetDesc());
                if(shader == shaders.end())
                    continue;
                newDesc.PS = *shader;
                updateConfig(g_RenderResources.renderConfigs, desc, newDesc);
                pipelines.push_back(renderer.GetDevice()->CreateGraphicsPipeline(newDesc, g_RenderResources.framebuffer));
            }

            for(auto& pipeline : pipelines) {
                g_RenderResources.psoCache[pipeline->GetDesc()] = std::move(pipeline);
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
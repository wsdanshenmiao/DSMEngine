#pragma once
#ifndef __SETUPPASS_H__
#define __SETUPPASS_H__

#include <random>
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

            g_RenderResources.cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("Global Command List"));

            bool reverseZ = renderer.GetCamera().IsReversedZ();
            sm_Sampler = renderer.GetDevice()->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Wrap)
                .SetComparisonFunc(reverseZ ? ComparisonFunc::Greater : ComparisonFunc::Less));

            auto& noiseTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Noise];
            noiseTex = device->CreateTexture(TextureDesc()
                .SetWidth(256)
                .SetHeight(256)
                .SetFormat(Format::RGBA8_UNORM)
                .SetDebugName("NoiseTex"));
            // 获取随机值
            std::array<uint8_t, 256 * 256 * 4> noiseData;
            std::mt19937 gen{std::random_device{}()};
            std::uniform_int_distribution<int> dist(0, std::numeric_limits<uint8_t>::max());
            for (size_t i = 0; i < noiseData.size(); ++i) {
                noiseData[i] = static_cast<uint8_t>(dist(gen));
            }
            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("SetupPass Noise Upload"));
            cmdList->Open();
            auto rowPitch = GetRowPitch(noiseTex->GetDesc().format, noiseTex->GetDesc().width);
            cmdList->WriteTexture(noiseTex, 0, 0, noiseData.data(), rowPitch);
            cmdList->Close();
            device->ExecuteCommandList(cmdList);

            //CreateShader(renderer);

            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Common].AddItem(BindingLayoutItem().Sampler(0));   // 默认采样器
            g_RenderResources.commonBindingSetDesc.AddItem(BindingSetItem().Sampler(0, sm_Sampler));

            const auto& bufferDesc = renderer.GetCurrentBackBuffer()->GetDesc();
            OnResize(renderer, bufferDesc.width, bufferDesc.height);
        }

        void Render(DSM::Renderer& renderer, float deltaTime) override {}
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            auto& colorTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Color];
            auto& depthTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Depth];
            // Resize color and depth texture
            colorTex = renderer.GetDevice()->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(renderer.GetCurrentBackBuffer()->GetDesc().format)
                .SetClearValue(Color{1, 0.7f, 0.75f, 1})
                .SetInitialState(ResourceStates::RenderTarget)
                .SetIsRenderTarget(true)
                .SetDebugName("ColorTex"));
            depthTex = renderer.GetDevice()->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::D32)
                .SetClearValue(Color{1, 0, 0, 0})
                .SetInitialState(ResourceStates::DepthWrite)
                .SetIsRenderTarget(true)    // 深度纹理也需要设置
                .SetDebugName("DepthTex"));
            g_RenderResources.framebuffer = renderer.GetDevice()->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(colorTex).SetDepthAttachment(depthTex));

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
                .SetFilename("Shaders/Passes/LitPass.hlsl")
                .SetEnterPoint("LitPassVS");
            ShaderByteCode litVSNoTangent{litVSDesc};
            ShaderByteCode litVS{litVSDesc.AddDefine("USE_TANGENT", "1")};

            ShaderCompileDesc litPSDesc{};
            litPSDesc.SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/Passes/LitPass.hlsl")
                .SetEnterPoint("LitPassPS");
            ShaderByteCode litPSNoTangent{litPSDesc};
            ShaderByteCode litPS{litPSDesc.AddDefine("USE_TANGENT", "1")};


            // 编译 ShadowPass 的着色器
            auto shadowVSDesc = ShaderCompileDesc()
                .SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_6)
                .SetEnterPoint("ShadowPassVS")
                .SetFilename("Shaders/Passes/ShadowPass.hlsl");
            ShaderByteCode shadowVS{shadowVSDesc};
            shadowVSDesc.AddDefine("SHADOWS_CLIP", "1");
            ShaderByteCode shadowVSClip{shadowVSDesc};
            auto shadowPSDesc = ShaderCompileDesc()
                .SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_6)
                .SetEnterPoint("ShadowPassPS")
                .SetFilename("Shaders/Passes/ShadowPass.hlsl");
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
    };

} // namespace DSM


#endif
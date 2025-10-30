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

            CreateSamplers(renderer);

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

            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Common].AddItem(
                BindingLayoutItem::Sampler(uint32_t(SamplerSlot::AnisoWrap)));   // 默认采样器
            g_RenderResources.commonBindingSetDesc.AddItem(
                BindingSetItem::Sampler(uint32_t(SamplerSlot::AnisoWrap), GetCommonSampler(SamplerSlot::AnisoWrap)));

            // SSAO
            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Common]
                .AddItem(BindingLayoutItem::Texture_SRV(LitPassBindingLayout::ShaderResource::SSAO));

            const auto& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, (uint32_t)viewport.Width(), (uint32_t)viewport.Height());
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
            auto preFramebuffer = g_RenderResources.framebuffer;
            g_RenderResources.framebuffer = renderer.GetDevice()->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(colorTex).SetDepthAttachment(depthTex));

            for(auto& [desc, pipeline] : g_RenderResources.psoCache){
                if(pipeline->GetFramebufferInfo() == preFramebuffer->GetFramebufferInfo()){
                    pipeline = renderer.GetDevice()->CreateGraphicsPipeline(desc, g_RenderResources.framebuffer);
                }
            }
        }
        
        void CreateSamplers(Renderer& renderer) 
        {
            auto device = renderer.GetDevice();
            bool reverseZ = renderer.GetCamera().IsReversedZ();
            auto& samplers = g_RenderResources.commonSamplers;

            samplers[uint8_t(SamplerSlot::PointClamp)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Clamp)
                .SetAllFilters(false));
            samplers[uint8_t(SamplerSlot::LinearClamp)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Clamp)
                .SetAllFilters(true));
            samplers[uint8_t(SamplerSlot::AnisoClamp)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Clamp)
                .SetMaxAnisotropy(4));
            samplers[uint8_t(SamplerSlot::PointWrap)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Wrap)
                .SetAllFilters(false));
            samplers[uint8_t(SamplerSlot::LinearWrap)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Wrap)
                .SetAllFilters(true));
            samplers[uint8_t(SamplerSlot::AnisoWrap)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Wrap)
                .SetMaxAnisotropy(4));
            samplers[uint8_t(SamplerSlot::PointBorder)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Border)
                .SetAllFilters(false));
            samplers[uint8_t(SamplerSlot::LinearBorder)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Border)
                .SetAllFilters(true));
            samplers[uint8_t(SamplerSlot::Shadow)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Border)
                .SetAllFilters(false)   // 点采样
                .SetComparisonFunc(ComparisonFunc::LessOrEqual)
                .SetReductionType(SamplerReductionType::Comparison));
        }

    };

} // namespace DSM


#endif
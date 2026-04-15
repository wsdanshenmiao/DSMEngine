#pragma once
#ifndef __TAA_PASS_H__
#define __TAA_PASS_H__

#include "RenderResource.h"
#include "Runtime/Math/MathCommon.h"
#include "Runtime/Render/ShaderCompiler.h"

namespace DSM {
    class TaaPass : public IRenderPass
    {
    public:
        struct TaaSettings
        {
            size_t sampleCount = 8;
            float baseHistoryWeight = 0.9f;
            float varianceClip = 6.0f;
        };

        TaaPass(GraphicsRenderer& renderer)
        {
            auto device = renderer.GetDevice();
            m_TaaBindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .SetVisibility(ShaderType::Pixel)
                .AddItem(BindingLayoutItem::PushConstants(0,  sizeof(TaaConstants)))
                .AddItem(BindingLayoutItem::Texture_SRV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(1))
                .AddItem(BindingLayoutItem::Texture_SRV(2))
                .AddItem(BindingLayoutItem::Sampler((uint32_t)SamplerSlot::LinearClamp))
                .AddItem(BindingLayoutItem::Sampler((uint32_t)SamplerSlot::PointClamp)));

            auto createShader = [device](ShaderType type, const std::string& entryPoint) {
                auto byteCode = ShaderByteCode{ShaderCompileDesc()
                    .SetType(type)
                    .SetMode(ShaderMode::SM_6_6)
                    .SetFilename("Shaders/ForwardShader/Passes/TaaPass.hlsl")
                    .SetEnterPoint(entryPoint)};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(type)
                    .SetDebugName("TAA " + entryPoint)
                    .SetEntryName(entryPoint),
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };

            m_TaaVS = createShader(ShaderType::Vertex, "TaaPassVS");
            m_TaaPS = createShader(ShaderType::Pixel, "TaaPassPS");

            const auto& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, viewport.Width(), viewport.Height());
        }

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();
            auto colorTex = renderRes.GetCommonTexture(CommonTextureSlot::Color);
            if (colorTex == nullptr || m_HistoryColorTex == nullptr || m_CurrTex == nullptr) {
                return 0;
            }

            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("TAA Command List"));
            cmdList->Open();
            if(m_ResetHistory){
                cmdList->CopyTexture(m_HistoryColorTex, {}, colorTex, {});
            }
            else{
                ExecuteTaaPass(renderer, cmdList, colorTex);
            }
            cmdList->Close();

            m_ResetHistory = false;
            return device->ExecuteCommandList(cmdList);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            auto device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();

            auto colorTexDesc = renderRes.GetCommonTexture(CommonTextureSlot::Color)->GetDesc();
            m_HistoryColorTex = device->CreateTexture(colorTexDesc.SetDebugName("TAA History Color Texture"));
            m_CurrTex = device->CreateTexture(colorTexDesc.SetDebugName("TAA Output Texture"));

            auto pipelineDesc = GraphicsPipelineDesc{}
                .SetVertexShader(m_TaaVS)
                .AddBindingLayout(m_TaaBindingLayout, 0)
                .SetRenderState(RenderState()
                    .SetDepthStencilState(DepthStencilState()
                        .SetDepthWriteEnable(false)
                        .SetDepthTestEnable(false))
                    .SetRasterState(RasterState().SetCullMode(RasterCullMode::None)));
            auto fb = renderRes.GetFramebuffer();
            m_TaaPipeline = device->CreateGraphicsPipeline(pipelineDesc.SetPixelShader(m_TaaPS), fb);

            auto linearClampSampler = renderRes.GetCommonSampler(SamplerSlot::LinearClamp);
            auto pointClampSampler = renderRes.GetCommonSampler(SamplerSlot::PointClamp);
            m_TaaBindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::PushConstants(0, sizeof(TaaConstants)))
                .AddItem(BindingSetItem::Texture_SRV(0, m_CurrTex))
                .AddItem(BindingSetItem::Texture_SRV(1, m_HistoryColorTex))
                .AddItem(BindingSetItem::Texture_SRV(2, renderRes.GetCommonTexture(CommonTextureSlot::MotionVector)))
                .AddItem(BindingSetItem::Sampler((uint32_t)SamplerSlot::LinearClamp, linearClampSampler))
                .AddItem(BindingSetItem::Sampler((uint32_t)SamplerSlot::PointClamp, pointClampSampler))
                , m_TaaPipeline->GetDesc().bindingLayouts[0]);

            m_ResetHistory = true;
        }

        static const TaaSettings& GetSettings() { return sm_Settings; }
        void SetSettings(const TaaSettings& settings) { sm_Settings = settings; }

        static float Halton(uint32_t index, uint32_t base)
        {
            float result = 0.0f;
            float invBase = 1.0f / float(base);
            float fraction = invBase;
            uint32_t i = index + 1;

            while (i > 0) {
                result += float(i % base) * fraction;
                i /= base;
                fraction *= invBase;
            }
            return result;
        }

        static Math::Vector2 GetJitterOffset(uint32_t frameIndex)
        {
            // 使用 Halton 序列生成 2D 抖动偏移
            frameIndex %= std::max(sm_Settings.sampleCount, 1zu);
            float jitterX = Halton(frameIndex, 2) - 0.5f;
            float jitterY = Halton(frameIndex, 3) - 0.5f;
            return Math::Vector2{jitterX, jitterY};
        }

    private:
        void ExecuteTaaPass(GraphicsRenderer& renderer, ICommandList* cmdList, ITexture* colorTex)
        {
            auto device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();

            cmdList->CopyTexture(m_CurrTex, {}, colorTex, {});

            float blendFactor = std::clamp(sm_Settings.baseHistoryWeight, 0.0f, 0.99f);
            size_t sampleCount = std::max(sm_Settings.sampleCount, 1zu);
            // 根据历史帧的数量限制历史权重的最大值
            if (sampleCount > 0) {
                blendFactor = std::min(blendFactor, float(sampleCount - 1) / float(sampleCount));
            }

            cmdList->SetGraphicsState(GraphicsState()
                .SetPipeline(m_TaaPipeline)
                .AddBindingSet(m_TaaBindingSet, 0)
                .SetFramebuffer(renderRes.GetFramebuffer())
                .SetViewport(ViewportState().AddViewportAndScissorRect(renderer.GetCamera().GetViewPort())));

            TaaConstants constants{};
            constants.historyWeight = blendFactor;
            constants.varianceClip = std::max(sm_Settings.varianceClip, 0.0f);
            cmdList->SetPushConstants(&constants, sizeof(constants));

            cmdList->Draw(DrawArguments().SetVertexCount(3));

            // 拷贝历史帧
            cmdList->CopyTexture(m_HistoryColorTex, {}, colorTex, {});
        }

    private:
        struct TaaConstants
        {
            float historyWeight;
            float varianceClip;
        };

        inline static TaaSettings sm_Settings{};
        
        GraphicsPipelineHandle m_TaaPipeline{};

        BindingLayoutHandle m_TaaBindingLayout{};
        BindingSetHandle m_TaaBindingSet{};

        ShaderHandle m_TaaVS{};
        ShaderHandle m_TaaPS{};

        // 缓存的历史帧
        TextureHandle m_HistoryColorTex{};
        TextureHandle m_CurrTex{};

        bool m_ResetHistory = true;
    };
}

#endif // !__TAA_PASS_H__

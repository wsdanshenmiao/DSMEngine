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
            size_t sampleCount = 10;
            float baseHistoryWeight = 0.9f;
            float varianceClip = 100;
            bool enableColorClip = true;
            bool useYCoCg = true;
            bool useClosestFragment = true;
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
                .AddItem(BindingLayoutItem::Texture_SRV(3))
                .AddItem(BindingLayoutItem::Sampler((uint32_t)SamplerSlot::LinearClamp))
                .AddItem(BindingLayoutItem::Sampler((uint32_t)SamplerSlot::PointClamp)));

            auto createShader = [device](ShaderType type, const std::string& entryPoint, const std::vector<std::string>& defines) {
                auto compileDesc = ShaderCompileDesc()
                    .SetType(type)
                    .SetMode(ShaderMode::SM_6_6)
                    .SetFilename("Shaders/ForwardShader/Passes/TaaPass.hlsl")
                    .SetEnterPoint(entryPoint);
                for(const auto& define : defines) {
                    compileDesc.AddDefine(define, "1");
                }
                auto byteCode = ShaderByteCode{compileDesc};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(type)
                    .SetDebugName("TAA " + entryPoint)
                    .SetEntryName(entryPoint),
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };

            m_TaaVS = createShader(ShaderType::Vertex, "TaaPassVS", {});
            for(size_t i = 0; i <= AllOptions; ++i) {
                std::vector<std::string> defines{};
                if(HasFlags(ShaderOptions(i), ShaderOptions::UseColorClip)) {
                    defines.push_back("USE_COLOR_CLIP");
                }
                if(HasFlags(ShaderOptions(i), ShaderOptions::UseYCoCg)) {
                    defines.push_back("USE_YCOCG");
                }
                if(HasFlags(ShaderOptions(i), ShaderOptions::ReverseZ)){
                    defines.push_back("REVERSED_Z");
                }
                if(HasFlags(ShaderOptions(i), ShaderOptions::UseClosestFragment)){
                    defines.push_back("USE_CLOSEST_FRAGMENT");
                }
                m_TaaPS[i] = createShader(ShaderType::Pixel, "TaaPassPS", defines);
            }

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

            for (size_t i = 0; i < m_TaaPipeline.size(); ++i) {
                m_TaaPipeline[i] = device->CreateGraphicsPipeline(GraphicsPipelineDesc{}
                    .SetVertexShader(m_TaaVS)
                    .SetPixelShader(m_TaaPS[i])
                    .AddBindingLayout(m_TaaBindingLayout, 0)
                    .SetRenderState(RenderState()
                        .SetDepthStencilState(DepthStencilState()
                            .SetDepthWriteEnable(false)
                            .SetDepthTestEnable(false))
                        .SetRasterState(RasterState().SetCullMode(RasterCullMode::None)))
                    , renderRes.GetFramebuffer());
            }

            auto linearClampSampler = renderRes.GetCommonSampler(SamplerSlot::LinearClamp);
            auto pointClampSampler = renderRes.GetCommonSampler(SamplerSlot::PointClamp);
            m_TaaBindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::PushConstants(0, sizeof(TaaConstants)))
                .AddItem(BindingSetItem::Texture_SRV(0, m_CurrTex))
                .AddItem(BindingSetItem::Texture_SRV(1, m_HistoryColorTex))
                .AddItem(BindingSetItem::Texture_SRV(2, renderRes.GetCommonTexture(CommonTextureSlot::MotionVector)))
                .AddItem(BindingSetItem::Texture_SRV(3, renderRes.GetCommonTexture(CommonTextureSlot::Depth)))
                .AddItem(BindingSetItem::Sampler((uint32_t)SamplerSlot::LinearClamp, linearClampSampler))
                .AddItem(BindingSetItem::Sampler((uint32_t)SamplerSlot::PointClamp, pointClampSampler))
                , m_TaaBindingLayout);

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

            size_t optionIndex = 0;
            if(sm_Settings.enableColorClip) {
                optionIndex |= ShaderOptions::UseColorClip;
            }
            if(sm_Settings.useYCoCg) {
                optionIndex |= ShaderOptions::UseYCoCg;
            }
            if(renderer.GetCamera().IsReversedZ()){
                optionIndex |= ShaderOptions::ReverseZ;
            }
            if(sm_Settings.useClosestFragment){
                optionIndex |= ShaderOptions::UseClosestFragment;
            }
            const auto& viewPort = renderer.GetCamera().GetViewPort();
            cmdList->SetGraphicsState(GraphicsState()
                .SetPipeline(m_TaaPipeline[optionIndex])
                .AddBindingSet(m_TaaBindingSet, 0)
                .SetFramebuffer(renderRes.GetFramebuffer())
                .SetViewport(ViewportState().AddViewportAndScissorRect(viewPort)));

            TaaConstants constants{};
            constants.historyWeight = blendFactor;
            constants.varianceClip = std::max(sm_Settings.varianceClip, 0.0f);
            constants.texelSizeX = 1.f / viewPort.Width();
            constants.texelSizeY = 1.f / viewPort.Height();
            cmdList->SetPushConstants(&constants, sizeof(constants));

            cmdList->Draw(DrawArguments().SetVertexCount(3));

            // 拷贝历史帧
            cmdList->CopyTexture(m_HistoryColorTex, {}, colorTex, {});
        }

    private:
        enum ShaderOptions
        {
            UseColorClip = 1 << 0,
            UseYCoCg = 1 << 1,
            ReverseZ = 1 << 2,
            UseClosestFragment = 1 << 3,
            AllOptions = (1 << 4) - 1
        };

        struct TaaConstants
        {
            float historyWeight;
            float varianceClip;
            float texelSizeX;
            float texelSizeY;
        };

        inline static TaaSettings sm_Settings{};
        
        std::array<GraphicsPipelineHandle, AllOptions + 1> m_TaaPipeline{};

        BindingLayoutHandle m_TaaBindingLayout{};
        BindingSetHandle m_TaaBindingSet{};

        ShaderHandle m_TaaVS{};
        std::array<ShaderHandle, AllOptions + 1> m_TaaPS{};

        // 缓存的历史帧
        TextureHandle m_HistoryColorTex{};
        TextureHandle m_CurrTex{};

        bool m_ResetHistory = true;
    };
}

#endif // !__TAA_PASS_H__

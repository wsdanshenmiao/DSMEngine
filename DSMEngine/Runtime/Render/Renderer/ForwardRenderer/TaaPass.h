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
            m_BindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .SetVisibility(ShaderType::Pixel)
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem::Texture_SRV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(1))
                .AddItem(BindingLayoutItem::Texture_SRV(2))
                .AddItem(BindingLayoutItem::Sampler((uint32_t)SamplerSlot::LinearClamp))
                .AddItem(BindingLayoutItem::Sampler((uint32_t)SamplerSlot::PointClamp)));
            m_ConstantBuffer = device->CreateBuffer(BufferDesc()
                .SetDebugName("TAA Constant Buffer")
                .SetByteSize(sizeof(TaaConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true));

            auto createShader = [device](ShaderType type, const std::string& filename, const std::string& entryPoint) {
                auto byteCode = ShaderByteCode{ShaderCompileDesc()
                    .SetType(type)
                    .SetMode(ShaderMode::SM_6_6)
                    .SetFilename(filename)
                    .SetEnterPoint(entryPoint)};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(type)
                    .SetDebugName("TAA " + entryPoint)
                    .SetEntryName(entryPoint),
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };

            std::string taaFilePath = "Shaders/ForwardShader/Passes/TaaPass.hlsl";
            m_VertexShader = createShader(ShaderType::Vertex, taaFilePath, "TaaPassVS");
            m_TaaPS = createShader(ShaderType::Pixel, taaFilePath, "TaaPassPS");
            m_MotionVecPS = createShader(ShaderType::Pixel, "Shaders/ForwardShader/MotionVector.hlsl", "MotionVectorPS");

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
                m_PreViewProjMatrix = Math::Matrix4::Transpose(renderer.GetCamera().GetViewProjMatrix());
                cmdList->CopyTexture(m_HistoryColorTex, {}, colorTex, {});
            }
            else{
                ExecuteMotionVectorPass(renderer, cmdList);
                ExecuteTaaPass(renderer, cmdList, colorTex);
            }
            cmdList->Close();

            m_ResetHistory = false;
            return device->ExecuteCommandList(cmdList);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            auto device = renderer.GetDevice();
            auto colorTexDesc = RenderResource::GetInstance().GetCommonTexture(CommonTextureSlot::Color)->GetDesc();
            m_HistoryColorTex = device->CreateTexture(colorTexDesc.SetDebugName("TAA History Color Texture"));
            m_CurrTex = device->CreateTexture(colorTexDesc.SetDebugName("TAA Output Texture"));
            auto motinVecDesc = colorTexDesc;
            m_MotionVecTex = device->CreateTexture(motinVecDesc
                .SetDebugName("TAA Motion Vector Texture")
                .SetFormat(Format::RG16_FLOAT)
                .SetClearValue({0, 0, 0, 0})
                .SetIsRenderTarget(true));

            m_MotionVecFB = device->CreateFramebuffer(FramebufferDesc().AddColorAttachment(m_MotionVecTex, AllSubresources));

            auto pipelineDesc = GraphicsPipelineDesc{}
                .SetVertexShader(m_VertexShader)
                .AddBindingLayout(m_BindingLayout, 0)
                .SetRenderState(RenderState()
                    .SetDepthStencilState(DepthStencilState()
                        .SetDepthWriteEnable(false)
                        .SetDepthTestEnable(false))
                    .SetRasterState(RasterState().SetCullMode(RasterCullMode::None)));
            auto fb = RenderResource::GetInstance().GetFramebuffer();
            m_TaaPipeline = device->CreateGraphicsPipeline(pipelineDesc.SetPixelShader(m_TaaPS), fb);
            m_MotionVecPipeline = device->CreateGraphicsPipeline(pipelineDesc.SetPixelShader(m_MotionVecPS), m_MotionVecFB);

            auto linearClampSampler = RenderResource::GetInstance().GetCommonSampler(SamplerSlot::LinearClamp);
            m_TaaBindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::ConstantBuffer(0, m_ConstantBuffer))
                .AddItem(BindingSetItem::Texture_SRV(0, m_CurrTex))
                .AddItem(BindingSetItem::Texture_SRV(1, m_HistoryColorTex))
                .AddItem(BindingSetItem::Texture_SRV(2, m_MotionVecTex))
                .AddItem(BindingSetItem::Sampler((uint32_t)SamplerSlot::LinearClamp, linearClampSampler))
                , m_TaaPipeline->GetDesc().bindingLayouts[0]);
            
            auto pointClampSampler = RenderResource::GetInstance().GetCommonSampler(SamplerSlot::PointClamp);
            m_MotionVecBindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::ConstantBuffer(0, m_ConstantBuffer))
                .AddItem(BindingSetItem::Texture_SRV(0, fb->GetDesc().depthAttachment.texture))
                .AddItem(BindingSetItem::Sampler((uint32_t)SamplerSlot::PointClamp, pointClampSampler))
                , m_MotionVecPipeline->GetDesc().bindingLayouts[0]);

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
        void ExecuteMotionVectorPass(GraphicsRenderer& renderer, ICommandList* cmdList)
        {
            auto view = renderer.GetCamera().GetViewMatrix();
            auto proj = renderer.GetCamera().GetProjMatrix();
            auto viewport = renderer.GetCamera().GetViewPort();
            auto jitter = GetJitterOffset(renderer.GetFrameIndex()) / Math::Vector2{viewport.Width(), viewport.Height()};
            proj.Set(2, 0, proj.Get(2, 0) + jitter.Get(0) * 2.f);
            proj.Set(2, 1, proj.Get(2, 1) + jitter.Get(1) * 2.f);

            auto currViewProj = Math::Matrix4::Transpose(view * proj);
            TaaConstants constants{};
            constants.prevViewProj = m_PreViewProjMatrix;
            constants.currInvViewProj = Math::Matrix4::Inverse(currViewProj);
            cmdList->WriteBuffer(m_ConstantBuffer, &constants, sizeof(constants));
            cmdList->ClearTextureFloat(m_MotionVecTex, AllSubresources, {0, 0, 0, 0});

            cmdList->SetGraphicsState(GraphicsState()
                .SetPipeline(m_MotionVecPipeline)
                .AddBindingSet(m_MotionVecBindingSet, 0)
                .SetFramebuffer(m_MotionVecFB)
                .SetViewport(ViewportState().AddViewportAndScissorRect(renderer.GetCamera().GetViewPort())));

            cmdList->Draw(DrawArguments().SetVertexCount(3));

            m_PreViewProjMatrix = currViewProj;
        }

        void ExecuteTaaPass(GraphicsRenderer& renderer, ICommandList* cmdList, ITexture* colorTex)
        {
            cmdList->CopyTexture(m_CurrTex, {}, colorTex, {});

            float blendFactor = std::clamp(sm_Settings.baseHistoryWeight, 0.0f, 0.99f);
            size_t sampleCount = std::max(sm_Settings.sampleCount, 1zu);
            // 根据历史帧的数量限制历史权重的最大值
            if (sampleCount > 0) {
                blendFactor = std::min(blendFactor, float(sampleCount - 1) / float(sampleCount));
            }

            TaaConstants constants{};
            constants.historyWeight = blendFactor;
            constants.varianceClip = std::max(sm_Settings.varianceClip, 0.0f);
            cmdList->WriteBuffer(m_ConstantBuffer, &constants, sizeof(constants));

            cmdList->SetGraphicsState(GraphicsState()
                .SetPipeline(m_TaaPipeline)
                .AddBindingSet(m_TaaBindingSet, 0)
                .SetFramebuffer(RenderResource::GetInstance().GetFramebuffer())
                .SetViewport(ViewportState().AddViewportAndScissorRect(renderer.GetCamera().GetViewPort())));

            cmdList->Draw(DrawArguments().SetVertexCount(3));

            // 拷贝历史帧
            cmdList->CopyTexture(m_HistoryColorTex, {}, colorTex, {});
        }

    private:
        struct TaaConstants
        {
            union
            {
                struct
                {
                    float historyWeight;
                    float varianceClip;
                    float pad[2];
                };
                struct
                {
                    Math::Matrix4 currInvViewProj;
                    Math::Matrix4 prevViewProj;
                };
            };
        };

        inline static TaaSettings sm_Settings{};
        
        GraphicsPipelineHandle m_TaaPipeline{};
        GraphicsPipelineHandle m_MotionVecPipeline{};

        BindingLayoutHandle m_BindingLayout{};
        BindingSetHandle m_TaaBindingSet{};
        BindingSetHandle m_MotionVecBindingSet{};

        ShaderHandle m_VertexShader{};
        ShaderHandle m_TaaPS{};
        ShaderHandle m_MotionVecPS{};

        bool m_ResetHistory = true;

        // 缓存的历史帧
        TextureHandle m_HistoryColorTex{};
        TextureHandle m_CurrTex{};
        TextureHandle m_MotionVecTex{};
        BufferHandle m_ConstantBuffer{};
        
        FramebufferHandle m_MotionVecFB{};

        Math::Matrix4 m_PreViewProjMatrix = Math::Matrix4::Identity;
    };
}

#endif // !__TAA_PASS_H__

#pragma once
#ifndef __FINALPASS_H__
#define __FINALPASS_H__

#include "IRenderPass.h"

namespace DSM {
    class FinalPass : public IRenderPass
    {
    public:
        FinalPass(Renderer& renderer, std::span<std::shared_ptr<Model>> models)
        {
            g_RenderResources.bindingLayout = renderer.GetDevice()->CreateBindingLayout(g_RenderResources.bindingLayoutDesc);

            for(const auto& model : models){
                GenerateRenderConfigs(renderer, model);
            }
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            auto cmdList = renderer.GetDevice()->CreateCommandList(
                CommandListParameters().SetDebugName("FinalPassCmdList"));
            cmdList->Open();

            auto backTexture = renderer.GetCurrentBackBuffer();
            cmdList->CopyTexture(backTexture, {}, g_RenderResources.framebuffer->GetDesc().colorAttachments[0].texture, {});

            cmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(cmdList);
            renderer.GetDevice()->RunGarbageCollection();
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override {}

    private:
        void GenerateRenderConfigs(Renderer& renderer, std::shared_ptr<Model> model)
        {
            auto device = renderer.GetDevice();
            // 创建渲染配置
            GraphicsPipelineHandle pipeline{};
            std::vector<VertexAttributeDesc> attributes{};
            attributes.reserve(4);

            BlendState hasBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{}.SetBlendEnable(true));
            BlendState noBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{});

            auto reverseZ = renderer.GetCamera().IsReversedZ();
            DepthStencilState readWriteDepth = DepthStencilState{}
                .SetDepthFunc(reverseZ ? ComparisonFunc::Greater : ComparisonFunc::Less);
            DepthStencilState readDepth = DepthStencilState{}
                .SetDepthWriteEnable(false)
                .SetDepthFunc(reverseZ ? ComparisonFunc::Greater : ComparisonFunc::Less);

            RasterState defaultRaster = RasterState{};
            RasterState twoSided = RasterState{}.SetCullMode(RasterCullMode::None);

            auto addAttribute = [&attributes](auto currFlag, auto flag, 
                const std::string& name, auto index, auto format, auto size) {
                if (HasFlags(PSOFlags(currFlag), flag)) {
                    attributes.push_back(VertexAttributeDesc()
                        .SetName(name)
                        .SetBufferIndex(index)
                        .SetFormat(format)
                        .SetElementStride(size));
                }
            };

            const auto& shaders = g_RenderResources.shaders;
            auto litVS = shaders[(size_t)ShaderSlot::LitVS];
            auto litVSNoTangent = shaders[(size_t)ShaderSlot::LitVSNoTangent];

            auto createPipeline = [&](bool hasTangent, const auto& layout, const auto& renderState) {
                GraphicsPipelineDesc retDesc{};
                for(size_t i = 0; i <= (size_t)ShaderSlot::LitPSPCF7 - (size_t)ShaderSlot::LitPS; ++i){
                    ShaderHandle ps = hasTangent ? shaders[size_t(ShaderSlot::LitPS) + i] : 
                        shaders[size_t(ShaderSlot::LitPSNoTangent) + i];
                    auto desc = GraphicsPipelineDesc()
                        .SetInputLayout(layout)
                        .SetVertexShader(hasTangent ? litVS : litVSNoTangent)
                        .SetPixelShader(ps)
                        .SetRenderState(renderState)
                        .AddBindingLayout(g_RenderResources.bindingLayout);

                    if(!g_RenderResources.psoCache.contains(desc)){
                        g_RenderResources.psoCache[desc] = device->CreateGraphicsPipeline(desc, g_RenderResources.framebuffer);
                    }
                    if(ShadowPass::sm_Setting.directionalSetting.filter == i){
                        retDesc = desc;
                    }
                }
                return retDesc;
            };

            for(auto& mesh : model->meshes){
                attributes.clear();
                addAttribute(mesh->psoFlags, kHasPosition, 
                    "POSITION", 0, Format::RGB32_FLOAT, sizeof(Math::Vector3));
                addAttribute(mesh->psoFlags, kHasUV, 
                    "TEXCOORD", 1, Format::RG32_FLOAT, sizeof(Math::Vector2));
                addAttribute(mesh->psoFlags, kHasNormal, 
                    "NORMAL", 2, Format::RGB32_FLOAT, sizeof(Math::Vector3));
                addAttribute(mesh->psoFlags, kHasTangent, 
                    "TANGENT", 3, Format::RGBA32_FLOAT, sizeof(Math::Vector4));
                
                bool hasTangent = HasFlags(PSOFlags(mesh->psoFlags), kHasTangent);

                InputLayoutHandle layout = device->CreateInputLayout(attributes, hasTangent ? litVS : litVSNoTangent);

                const auto& blendState = HasFlags(PSOFlags(mesh->psoFlags), kAlphaBlend) ? hasBlend : noBlend;
                const auto& depthState = HasFlags(PSOFlags(mesh->psoFlags), kAlphaBlend) ? readDepth : readWriteDepth;
                const auto& rasterState = HasFlags(PSOFlags(mesh->psoFlags), kBothSide) ? twoSided : defaultRaster;

                auto desc = createPipeline(hasTangent, layout, RenderState{ blendState, depthState, rasterState });

                auto buffer = device->CreateBuffer(BufferDesc()
                    .SetDebugName(mesh->name + "MeshConstants")
                    .SetByteSize(sizeof(MeshConstants))
                    .SetIsConstantBuffer(true)
                    .SetIsVolatile(true));
                mesh->psoIndex = g_RenderResources.renderConfigs.size();
                g_RenderResources.renderConfigs.push_back({ desc, buffer });
            }
        }
    };

} // namespace DSM


#endif
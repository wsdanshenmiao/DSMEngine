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
                GenerateRenderConfigs(renderer.GetDevice(), model);
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
        void GenerateRenderConfigs(IDevice* device, std::shared_ptr<Model> model)
        {
            // 创建渲染配置
            GraphicsPipelineHandle pipeline{};
            std::vector<VertexAttributeDesc> attributes{};
            attributes.reserve(4);

            BlendState hasBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{}.SetBlendEnable(true));
            BlendState noBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{});

            DepthStencilState readWriteDepth = DepthStencilState{};
            DepthStencilState readDepth = DepthStencilState{}.SetDepthWriteEnable(false);

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

            auto litVS = g_RenderResources.shaders[(size_t)ShaderSlot::LitVS];
            auto litVSNoTangent = g_RenderResources.shaders[(size_t)ShaderSlot::LitVSNoTangent];
            auto litPS = g_RenderResources.shaders[(size_t)ShaderSlot::LitPS];
            auto litPSNoTangent = g_RenderResources.shaders[(size_t)ShaderSlot::LitPSNoTangent];

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

                auto desc = GraphicsPipelineDesc()
                    .SetInputLayout(layout)
                    .SetVertexShader(hasTangent ? litVS : litVSNoTangent)
                    .SetPixelShader(hasTangent ? litPS : litPSNoTangent)
                    .SetRenderState(RenderState{ blendState, depthState, rasterState })
                    .AddBindingLayout(g_RenderResources.bindingLayout);
                auto buffer = device->CreateBuffer(BufferDesc()
                    .SetDebugName(mesh->name + "MeshConstants")
                    .SetByteSize(sizeof(MeshConstants))
                    .SetIsConstantBuffer(true)
                    .SetIsVolatile(true));
                mesh->psoIndex = g_RenderResources.renderConfigs.size();

                if(!g_RenderResources.psoCache.contains(desc)){
                    g_RenderResources.psoCache[desc] = device->CreateGraphicsPipeline(desc, g_RenderResources.framebuffer);
                }
                g_RenderResources.renderConfigs.push_back({ desc, buffer });
            }
        }
    };

} // namespace DSM


#endif
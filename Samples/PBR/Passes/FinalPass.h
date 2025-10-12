#pragma once
#ifndef __FINALPASS_H__
#define __FINALPASS_H__

#include "ShadowPass.h"

namespace DSM {
    class FinalPass : public IRenderPass
    {
    public:
        FinalPass(Renderer& renderer)
        {
            auto device = renderer.GetDevice();
            
            // 为前面 Pass 收集的 Desc 创建 Layout 和 Set
            for(size_t i = 0; i < (size_t)BindingLayoutSlot::Count; ++i) {
                g_RenderResources.bindingLayouts[i] = device->CreateBindingLayout(g_RenderResources.bindingLayoutDescs[i]);
            }
            auto objs = DSMEngine::sm_GlobalContext.scene->GetAllObjectsWithComponents<Model>();
            for(const auto& obj : objs){
                auto& model = objs.get<Model>(obj);
                GenerateRenderConfigs(renderer, model);
            }

            sm_TimerQuery = renderer.GetDevice()->CreateTimerQuery();

            const Viewport& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, (uint32_t)viewport.Width(), (uint32_t)viewport.Height());
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            // auto cmdList = renderer.GetDevice()->CreateCommandList(
            //     CommandListParameters().SetDebugName("FinalPassCmdList"));
            auto& cmdList = g_RenderResources.cmdList;
            cmdList->Open();

            cmdList->BeginTimerQuery(sm_TimerQuery);

            ITexture* backTexture = renderer.GetCurrentBackBuffer();
            TextureHandle colorTex = GetCommonTexture(CommonTextureSlot::Color);
            cmdList->CopyTexture(backTexture, {}, colorTex, {});
            cmdList->SetTextureState(colorTex, AllSubresources, ResourceStates::ShaderResource);

            cmdList->EndTimerQuery(sm_TimerQuery);
            
            cmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(cmdList);
            renderer.GetDevice()->RunGarbageCollection();
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override 
        {
            g_RenderResources.commonBindingSet = renderer.GetDevice()->CreateBindingSet(
                g_RenderResources.commonBindingSetDesc, g_RenderResources.bindingLayouts[(size_t)BindingLayoutSlot::Common]);
        }

    private:
        void GenerateRenderConfigs(Renderer& renderer, const Model& model)
        {
            auto device = renderer.GetDevice();
            // 创建渲染配置
            GraphicsPipelineHandle pipeline{};
            std::vector<VertexAttributeDesc> attributes{};
            attributes.reserve(4);

            BlendState hasBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{}.SetBlendEnable(true));
            BlendState noBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{});

            auto reverseZ = renderer.GetCamera().IsReversedZ();
            DepthStencilState readDepth = DepthStencilState{}
                .SetDepthWriteEnable(false)
                .SetDepthFunc(reverseZ ? ComparisonFunc::GreaterOrEqual : ComparisonFunc::LessOrEqual);

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
                        .SetRenderState(renderState);
                    for(size_t i = 0; i < (size_t)BindingLayoutSlot::Count; ++i) {
                        desc.AddBindingLayout(g_RenderResources.bindingLayouts[i], i);
                    }

                    if(!g_RenderResources.psoCache.contains(desc)){
                        g_RenderResources.psoCache[desc] = device->CreateGraphicsPipeline(desc, g_RenderResources.framebuffer);
                    }
                    if(ShadowPass::sm_Setting.directionalSetting.filter == i){
                        retDesc = desc;
                    }
                }
                return retDesc;
            };

            for(auto& mesh : model.meshes){
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
                const auto& depthState = readDepth;
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


    public:
        inline static TimerQueryHandle sm_TimerQuery{};
    };

} // namespace DSM


#endif
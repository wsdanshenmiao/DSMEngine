#pragma once
#ifndef __SETUPPASS_H__
#define __SETUPPASS_H__

#include <random>
#include "ShadowPass.h"
#include "Runtime/Render/Renderer/Renderer.h"
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

        void Render(DSM::Renderer& renderer, float deltaTime) override 
        {
            auto objs = DSMEngine::sm_GlobalContext.scene->GetAllObjectsWithComponents<Model>();
            std::set<const Model*> pModels{};
            bool generateNeeded = false;
            for(const auto& obj : objs){
                auto& model = objs.get<Model>(obj);
                if(!m_pModels.contains(&model) && model.meshes.size() > 0){
                    pModels.insert(&model);
                    generateNeeded = true;
                }
            }
            if(generateNeeded){
                g_RenderResources.renderConfigs.clear();
                for(const auto& obj : objs){
                    GenerateRenderConfigs(renderer, objs.get<Model>(obj));
                }
            }
            m_pModels = std::move(pModels);
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            auto colorTex = renderer.GetColorTexture();
            auto& depthTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Depth];
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


    private:
        std::set<const Model*> m_pModels{};
    };

} // namespace DSM


#endif
#pragma once
#ifndef __SHADOW_PASS_H__
#define __SHADOW_PASS_H__

#include <map>
#include "SetupPass.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Render/Model.h"
#include "Shaders/ResourceData.h"

namespace DSM {
    struct ShadowSetting
    {
        enum MapSize
        {
            _256 = 256,
            _512 = 512,
            _1024 = 1024,
            _2048 = 2048
        };
        
        enum FilterMode
        {
            None,
            _PCF3x3,
            _PCF5x5,
            _PCF7x7
        };
        
        struct Directional
        {
            MapSize size = MapSize::_1024;
            FilterMode filter = FilterMode::_PCF3x3;
        };

        float distance = 1000;
        float distanceFade = 0.1f;
        Directional directionalSetting{};
    };

    class ShadowPass : public IRenderPass
    {
    public:
        ShadowPass(Renderer& renderer, 
            ShadowSetting shadowSetting, 
            std::span<std::shared_ptr<Model>> models)
            : m_Models(models.begin(), models.end())
        {
            sm_Setting = shadowSetting;
            auto device = renderer.GetDevice();

            m_ShadowCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(ShadowConstants))
                .SetIsConstantBuffer(true)
                .SetDebugName("Shadow Constants Buffer"));

            m_ShadowMap = renderer.GetDevice()->CreateTexture(TextureDesc()
                .SetWidth(shadowSetting.directionalSetting.size)
                .SetHeight(shadowSetting.directionalSetting.size)
                .SetClearValue(Color{1,1,1,1})
                .SetFormat(Format::D32)
                .SetIsRenderTarget(true)
                .SetDebugName("ShadowMap"));

            m_ShadowFramebuffer = device->CreateFramebuffer(FramebufferDesc().SetDepthAttachment(m_ShadowMap));

            // ShadowMap 的采样器
            m_ShadowSampler = renderer.GetDevice()->CreateSampler(SamplerDesc()
                .SetMipFilter(false)   // 点采样
                .SetComparisonFunc(ComparisonFunc::LessOrEqual)
                .SetAllAddressModes(SamplerAddressMode::Border)
                .SetReductionType(SamplerReductionType::Comparison));


            std::array<VertexAttributeDesc, 2> vertexDescs = {
                VertexAttributeDesc()
                    .SetName("POSITION")
                    .SetFormat(Format::RGBA32_FLOAT)
                    .SetBufferIndex(0)
                    .SetElementStride(sizeof(Math::Vector4)),
                VertexAttributeDesc()
                    .SetName("TEXCOORD")
                    .SetFormat(Format::RG32_FLOAT)
                    .SetBufferIndex(1)
                    .SetElementStride(sizeof(Math::Vector2))
            };

            auto bindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(1))
                .AddItem(BindingLayoutItem().Texture_SRV(0))
                .AddItem(BindingLayoutItem().Sampler(0))
            );


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

            auto createShader = [&](const ShaderByteCode& byteCode, const auto& name) {
                return device->CreateShader(ShaderDesc()
                    .SetEntryName(byteCode.GetDesc().enterPoint)
                    .SetShaderType(byteCode.GetDesc().type)
                    .SetDebugName(name), 
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };
            auto& shaders = g_RenderResources.shaders;            
            shaders[size_t(ShaderSlot::ShadowVS)] = createShader(shadowVS, "ShadowPassVS");
            shaders[size_t(ShaderSlot::ShadowVSClip)] = createShader(shadowVSClip, "ShadowPassVSClip");
            shaders[size_t(ShaderSlot::ShadowPS)] = createShader(shadowPS, "ShadowPassPS");
            shaders[size_t(ShaderSlot::ShadowPSClip)] = createShader(shadowPSClip, "ShadowPassPSClip");

            // 为不同的 PCF 选项创建 PSO
            auto createPipeline = [&](IShader* vsHandle, IShader* psHandle, bool clip, const auto& renderState) {
                auto pipelineDesc = GraphicsPipelineDesc()
                    .SetVertexShader(vsHandle)
                    .SetPixelShader(psHandle)
                    .SetInputLayout(device->CreateInputLayout(vertexDescs, vsHandle))
                    .SetRenderState(renderState)
                    .AddBindingLayout(bindingLayout);
                g_RenderResources.psoCache[pipelineDesc] = device->CreateGraphicsPipeline(pipelineDesc, m_ShadowFramebuffer);
                return pipelineDesc;
            };
            size_t count = (ShadowSetting::FilterMode::_PCF7x7 + 1);
            for(size_t i = 0; i < 2 * count; ++i){
                bool clip = i < count;
                ShaderHandle vs = shaders[clip ? size_t(ShaderSlot::ShadowVSClip) : size_t(ShaderSlot::ShadowVS)];
                ShaderHandle ps = shaders[clip ? size_t(ShaderSlot::ShadowPSClip) : size_t(ShaderSlot::ShadowPS)];
                auto renderState = RenderState{};
                float scale = (i % count) + 1;
                renderState.rasterState.SetSlopeScaleDepthBias(1.5f * scale).SetDepthBias(100 * scale);
                auto desc = createPipeline(vs, ps, clip, renderState);
                m_ShadowPipelineDescs.push_back(desc);
            }

            // 将 ShadowMap 绑定到管线
            g_RenderResources.bindingLayoutDesc
                .AddItem(BindingLayoutItem().Texture_SRV(7))
                .AddItem(BindingLayoutItem().ConstantBuffer(4))
                .AddItem(BindingLayoutItem().Sampler(1));
            g_RenderResources.bindingSetDesc
                .AddItem(BindingSetItem().Texture_SRV(7, m_ShadowMap))
                .AddItem(BindingSetItem().ConstantBuffer(4, m_ShadowCB))
                .AddItem(BindingSetItem().Sampler(1, m_ShadowSampler));

            m_CmdList = device->CreateCommandList(CommandListParameters().SetDebugName("ShadowPass"));
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            m_DirectionalLights.clear();
            for(const auto& light : g_RenderResources.lights){
                if(light.lightType == LightType::Directional){
                    m_DirectionalLights.push_back(light);
                }
            }

            auto dirLightCount = std::min(m_DirectionalLights.size(), sm_MaxShadowedDirectionalLightCount);
            size_t tiles = dirLightCount;
            // 拆分为多少块
            size_t split = tiles <= 1 ? 1 : (tiles <= 4 ? 2 : 4);
            size_t tileSize = sm_Setting.directionalSetting.size / split;

            m_CmdList->Open();

            for(size_t i = 0; i < dirLightCount; ++i){
                RenderDirectionalShadow(renderer, i, split, tileSize);
            }
            // 转换为着色器资源以供后续 Pass 使用
            m_CmdList->SetTextureState(m_ShadowMap, AllSubresources, ResourceStates::ShaderResource);

            // 写入阴影变换矩阵
            ShadowConstants shadowConstants{};
            for(size_t i = 0; i < dirLightCount; ++i){
                shadowConstants.shadowViewProjs[i] = m_DirectionalShadowMatrices[i];
            }
            m_CmdList->WriteBuffer(m_ShadowCB, &shadowConstants, sizeof(ShadowConstants));

            m_CmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(m_CmdList);
        }
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override{}


    private:
        void RenderDirectionalShadow(Renderer& renderer, size_t index, size_t split, size_t tileSize)
        {
            auto device = renderer.GetDevice();

            m_CmdList->ClearDepthStencilTexture(m_ShadowMap, AllSubresources, true, 1, false, 0);
            
            Light& light = m_DirectionalLights[index];
            struct ShadowPassCB
            {
                Math::Matrix4 viewProj;
                Math::Vector4 baseColor;
            } shadowCB{};
            // TODO:后续根据场景物体的包围盒计算
            // 需要使用正交投影
            Camera lightCamera{};
            float len = 10;
            auto lightPos = -light.transform.GetForwardAxis() * len;
            Math::Vector4 center{0,0,0,1};
            lightCamera.SetPosition(lightPos);
            lightCamera.LookAt(Math::Vector3{center}, {0,1,0});
            auto view = lightCamera.GetViewMatrix();
            center = center * view;
            auto proj = Math::GetOrthographicMatrix(
                center.Get(0) - len, center.Get(0) + len, 
                center.Get(1) - len, center.Get(1) + len, 
                center.Get(2) - 2 * len, center.Get(2) + 2 * len);
            shadowCB.viewProj = Math::Matrix4::Transpose(view * proj);
            m_DirectionalShadowMatrices[index] = shadowCB.viewProj;

            for(const auto& model : m_Models){
                for(const auto& mesh : model->meshes){
                    bool alphaClip = HasFlags(PSOFlags(mesh->psoFlags), PSOFlags::kAlphaBlend);
                    for(const auto& [name, submesh] : mesh->subMeshes){
                        auto cbDesc = BufferDesc()
                            .SetByteSize(sizeof(ShadowPassCB))
                            .SetIsConstantBuffer(true)
                            .SetIsVolatile(true)
                            .SetDebugName("Shadow Pass CB");
                        auto shadowConstantBuffer = device->CreateBuffer(cbDesc);
                        shadowCB.baseColor = model->materials[submesh.materialIndex]->baseColor;
                        m_CmdList->WriteBuffer(shadowConstantBuffer, &shadowCB, sizeof(ShadowPassCB));

                        MeshConstants meshCB{};
                        meshCB.world = Math::Matrix4::Transpose(model->transform.GetLocalToWorld());
                        meshCB.worldIT = Math::Matrix4::Inverse(meshCB.world);
                        cbDesc.SetByteSize(sizeof(MeshConstants)).SetDebugName("Mesh CB");
                        auto meshConstantBuffer = device->CreateBuffer(cbDesc);
                        m_CmdList->WriteBuffer(meshConstantBuffer, &meshCB, sizeof(MeshConstants));

                        size_t baseIndex = alphaClip ? 0 : (ShadowSetting::FilterMode::_PCF7x7  +1);
                        const auto& desc = m_ShadowPipelineDescs[baseIndex + sm_Setting.directionalSetting.filter];
                        const auto& pipeline = g_RenderResources.psoCache[desc];

                        auto bindingSet = device->CreateBindingSet(BindingSetDesc()
                            .AddItem(BindingSetItem().ConstantBuffer(0, meshConstantBuffer))
                            .AddItem(BindingSetItem().ConstantBuffer(1, shadowConstantBuffer))
                            .AddItem(BindingSetItem().Texture_SRV(0, submesh.textures[kBaseColor]))
                            .AddItem(BindingSetItem().Sampler(0, SetupPass::sm_Sampler))
                            , pipeline->GetDesc().bindingLayouts[0]);

                        GraphicsState state{};
                        state.SetFramebuffer(m_ShadowFramebuffer)
                            .SetPipeline(pipeline)
                            .SetViewport(ViewportState().AddViewportAndScissorRect(GetTileViewport(index, split, tileSize)))
                            .SetIndexBuffer(mesh->indexBufferViews)
                            .AddVertexBuffer(mesh->positionStream)
                            .AddVertexBuffer(mesh->uvStream)
                            .AddBindingSet(bindingSet);

                        m_CmdList->SetGraphicsState(state);

                        m_CmdList->DrawIndexed(DrawArguments()
                            .SetVertexCount(submesh.indexCount)
                            .SetStartIndexLocation(submesh.indexOffset)
                            .SetStartVertexLocation(submesh.vertexOffset));
                    }
                }
            }
        }

        Viewport GetTileViewport(size_t index, size_t split, size_t tileSize) const
        {
            size_t x = (index % split) * tileSize;
            size_t y = (index / split) * tileSize;
            return Viewport(x, x + tileSize, y, y + tileSize, 0, 1);
        }


    public:
        inline static ShadowSetting sm_Setting;

    private:
        static constexpr size_t sm_MaxShadowedDirectionalLightCount = 4;
        CommandListHandle m_CmdList;

        TextureHandle m_ShadowMap;
        SamplerHandle m_ShadowSampler;

        BufferHandle m_ShadowCB;
        std::array<Math::Matrix4, sm_MaxShadowedDirectionalLightCount> m_DirectionalShadowMatrices{};

        FramebufferHandle m_ShadowFramebuffer;

        std::vector<GraphicsPipelineDesc> m_ShadowPipelineDescs;

        std::vector<Light> m_DirectionalLights{};

        std::vector<std::shared_ptr<Model>> m_Models;
    };
} // namespace DSM


#endif
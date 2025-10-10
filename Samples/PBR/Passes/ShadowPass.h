#pragma once
#ifndef __SHADOW_PASS_H__
#define __SHADOW_PASS_H__

#include <map>
#include "SetupPass.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Render/Model.h"
#include "Shaders/ResourceData.h"
#include "Runtime/Math/Collision/BoundingSphere.h"

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
        ShadowPass(Renderer& renderer, ShadowSetting shadowSetting)
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

            m_ShadowBindingLayouts[0] = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem().Sampler(uint32_t(SamplerSlot::AnisoWrap)))
            );
            m_ShadowBindingLayouts[1] = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(1))
                .AddItem(BindingLayoutItem().Texture_SRV(0))
            );

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

            // 为不同的 PCF 选项创建 PSO
            auto createPipeline = [&](IShader* vsHandle, IShader* psHandle, bool clip, const auto& renderState) {
                auto pipelineDesc = GraphicsPipelineDesc()
                    .SetVertexShader(vsHandle)
                    .SetPixelShader(psHandle)
                    .SetInputLayout(device->CreateInputLayout(vertexDescs, vsHandle))
                    .SetRenderState(renderState)
                    .AddBindingLayout(m_ShadowBindingLayouts[0], 0)
                    .AddBindingLayout(m_ShadowBindingLayouts[1], 1);
                auto pso = device->CreateGraphicsPipeline(pipelineDesc, m_ShadowFramebuffer);
                g_RenderResources.psoCache[pipelineDesc] = pso;
                return pso;
            };
            size_t count = (ShadowSetting::FilterMode::_PCF7x7 + 1);
            for(size_t i = 0; i < 2 * count; ++i){
                bool clip = i < count;
                ShaderHandle vs = shaders[clip ? size_t(ShaderSlot::ShadowVSClip) : size_t(ShaderSlot::ShadowVS)];
                ShaderHandle ps = shaders[clip ? size_t(ShaderSlot::ShadowPSClip) : size_t(ShaderSlot::ShadowPS)];
                auto renderState = RenderState{};
                float scale = (i % count) + 1;
                renderState.rasterState.SetSlopeScaleDepthBias(1.5f * scale).SetDepthBias(100 * scale);
                auto pso = createPipeline(vs, ps, clip, renderState);
                m_ShadowPipelineDescs.push_back(pso);
            }

            // 将 ShadowMap 绑定到管线
            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Common]
                .AddItem(BindingLayoutItem().Texture_SRV(LitPassBindingLayout::ShaderResource::ShadowMap))
                .AddItem(BindingLayoutItem().Sampler(uint32_t(SamplerSlot::Shadow)))
                .AddItem(BindingLayoutItem().ConstantBuffer(LitPassBindingLayout::Constants::ShadowConstants));
            g_RenderResources.commonBindingSetDesc
                .AddItem(BindingSetItem().Texture_SRV(LitPassBindingLayout::ShaderResource::ShadowMap, m_ShadowMap))
                .AddItem(BindingSetItem().Sampler(uint32_t(SamplerSlot::Shadow), GetCommonSampler(SamplerSlot::Shadow)))
                .AddItem(BindingSetItem().ConstantBuffer(LitPassBindingLayout::Constants::ShadowConstants, m_ShadowCB));

            sm_TimerQuery = device->CreateTimerQuery();
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

            // auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("ShadowPass"));
            auto& cmdList = g_RenderResources.cmdList;
            cmdList->Open();

            // 开始计时
            cmdList->BeginTimerQuery(sm_TimerQuery);

            cmdList->ClearDepthStencilTexture(m_ShadowMap, AllSubresources, true, 1, false, 0);

            for(size_t i = 0; i < dirLightCount; ++i){
                RenderDirectionalShadow(renderer, cmdList, i, split, tileSize);
            }
            // 转换为着色器资源以供后续 Pass 使用
            cmdList->SetTextureState(m_ShadowMap, AllSubresources, ResourceStates::ShaderResource);

            // 写入阴影变换矩阵
            ShadowConstants shadowConstants{};
            for(size_t i = 0; i < dirLightCount; ++i){
                shadowConstants.shadowViewProjs[i] = m_DirectionalShadowMatrices[i];
            }
            cmdList->WriteBuffer(m_ShadowCB, &shadowConstants, sizeof(ShadowConstants));

            // 结束计时
            cmdList->EndTimerQuery(sm_TimerQuery);

            cmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(cmdList);
        }
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override{}


    private:
        void RenderDirectionalShadow(Renderer& renderer, CommandListHandle& cmdList, size_t index, size_t split, size_t tileSize)
        {
            auto device = renderer.GetDevice();
            
            Light& light = m_DirectionalLights[index];
            struct ShadowPassCB
            {
                Math::Matrix4 viewProj;
                Math::Vector4 baseColor;
            } shadowCB{};

            auto view = DSMEngine::sm_GlobalContext.world->GetAllObjectsWithComponents<Model>();
            // 计算所有模型的包围盒
            Math::AxisAlignedBox boundingBox{};
            for (const auto& obj : view) {
                const auto& model = view.get<Model>(obj);
                auto transBox = model.boundingBox * model.transform;
                boundingBox = Math::AxisAlignedBox::Union(boundingBox, transBox);
            }
            Math::BoundingSphere boundingSphere{boundingBox};

            Camera lightCamera{};
            float radius = boundingSphere.GetRadius();
            auto lightPos = -light.direction * 2 * radius;
            Math::Vector4 center{boundingSphere.GetCenter(), 1};
            lightCamera.SetPosition(lightPos);
            lightCamera.LookAt(Math::Vector3{center}, {0,1,0});

            auto viewMatrix = lightCamera.GetViewMatrix();
            center = center * viewMatrix;

            // 需要使用正交投影
            float l = center.Get(0) - radius;
            float b = center.Get(1) - radius;
            float n = center.Get(2) - radius;
            float r = center.Get(0) + radius;
            float t = center.Get(1) + radius;
            float f = center.Get(2) + radius;
            
            Math::Matrix4 proj = Math::GetOrthographicMatrix(l, r, b, t, n, f);
            shadowCB.viewProj = Math::Matrix4::Transpose(viewMatrix * proj);
            Math::Vector2 offset{float(index % split), float(index / split)};
            m_DirectionalShadowMatrices[index] = ConvertToAtlasMatrix(shadowCB.viewProj, offset, 1.f / split);

            for(const auto& obj : view) {
                const auto& model = view.get<Model>(obj);
                auto bufferSize = Math::Align(sizeof(ShadowPassCB), size_t(c_ConstantBufferOffsetSizeAlignment));
                BufferHandle shadowConstantsBuffer = device->CreateBuffer(BufferDesc()
                    .SetByteSize(bufferSize * model.materials.size())
                    .SetIsConstantBuffer(true)
                    .SetIsVolatile(true)
                    .SetDebugName("Shadow Pass CB"));
                std::vector<bool> writtenMaterials(model.materials.size(), false);

                MeshConstants meshCB{};
                meshCB.world = Math::Matrix4::Transpose(model.transform.GetLocalToWorld());
                meshCB.worldIT = Math::Matrix4::Inverse(meshCB.world);
                auto meshConstantBuffer = device->CreateBuffer(BufferDesc()
                    .SetByteSize(sizeof(MeshConstants))
                    .SetIsConstantBuffer(true)
                    .SetIsVolatile(true)
                    .SetDebugName("Mesh CB"));
                cmdList->WriteBuffer(meshConstantBuffer, &meshCB, sizeof(MeshConstants));

                // 减少 BindingSet 的复杂度
                auto commonBindingSet = device->CreateBindingSet(BindingSetDesc()
                            .AddItem(BindingSetItem().ConstantBuffer(0, meshConstantBuffer))
                            .AddItem(BindingSetItem().Sampler(uint32_t(SamplerSlot::AnisoWrap), GetCommonSampler(SamplerSlot::AnisoWrap))),
                            m_ShadowBindingLayouts[0]);

                for(const auto& mesh : model.meshes){
                    bool alphaClip = HasFlags(PSOFlags(mesh->psoFlags), PSOFlags::kAlphaBlend);
                    for(const auto& [name, submesh] : mesh->subMeshes){
                        auto bufferOffset = bufferSize * submesh.materialIndex;
                        // 避免重复写入
                        if(!writtenMaterials[submesh.materialIndex]){
                            shadowCB.baseColor = model.materials[submesh.materialIndex]->baseColor;
                            cmdList->WriteBuffer(shadowConstantsBuffer, &shadowCB, sizeof(ShadowPassCB), bufferOffset);
                            writtenMaterials[submesh.materialIndex] = true;
                        }

                        size_t baseIndex = alphaClip ? 0 : (ShadowSetting::FilterMode::_PCF7x7  +1);
                        const auto& pipeline = m_ShadowPipelineDescs[baseIndex + sm_Setting.directionalSetting.filter];

                        auto bindingSet = device->CreateBindingSet(BindingSetDesc()
                            .AddItem(BindingSetItem().ConstantBuffer(1, shadowConstantsBuffer, 
                                BufferRange{}.SetByteOffset(bufferOffset).SetByteSize(bufferSize)))
                            .AddItem(BindingSetItem().Texture_SRV(0, submesh.textures[kBaseColor]))
                            , m_ShadowBindingLayouts[1]);

                        GraphicsState state{};
                        state.SetFramebuffer(m_ShadowFramebuffer)
                            .SetPipeline(pipeline)
                            .SetViewport(ViewportState().AddViewportAndScissorRect(GetTileViewport(index, split, tileSize)))
                            .SetIndexBuffer(mesh->indexBufferViews)
                            .AddVertexBuffer(mesh->positionStream)
                            .AddVertexBuffer(mesh->uvStream)
                            .AddBindingSet(commonBindingSet, 0)
                            .AddBindingSet(bindingSet, 1);

                        cmdList->SetGraphicsState(state);

                        cmdList->DrawIndexed(DrawArguments()
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

        Math::Matrix4 ConvertToAtlasMatrix(
            const Math::Matrix4& m, 
            Math::Vector2 offset, 
            float scale) const
        {
            // 进行纹理坐标的转换后变换到对应 Tile
            Math::Matrix4 atlasTransform{
                Math::Vector4{scale * 0.5f, 0.f, 0.f, (offset.Get(0) + 0.5f) * scale},
                Math::Vector4{0.f, scale * -0.5f, 0.f, (offset.Get(1) + 0.5f) * scale},
                Math::Vector4{0.f, 0.f, 1.f, 0.f},
                Math::Vector4{0.f, 0.f, 0.f, 1.f}
            };

            return atlasTransform * m;
        }


    public:
        inline static ShadowSetting sm_Setting;
        inline static TimerQueryHandle sm_TimerQuery{};

    private:
        static constexpr size_t sm_MaxShadowedDirectionalLightCount = 4;

        TextureHandle m_ShadowMap;

        BufferHandle m_ShadowCB;
        std::array<Math::Matrix4, sm_MaxShadowedDirectionalLightCount> m_DirectionalShadowMatrices{};

        FramebufferHandle m_ShadowFramebuffer;

        std::vector<GraphicsPipelineHandle> m_ShadowPipelineDescs;
        std::array<BindingLayoutHandle, 2> m_ShadowBindingLayouts;

        std::vector<Light> m_DirectionalLights{};
    };
} // namespace DSM


#endif
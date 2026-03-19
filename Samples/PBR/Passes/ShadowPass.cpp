#include "ShadowPass.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Render/Model.h"
#include "../Shaders/ResourceData.h"
#include "Runtime/Math/Collision/BoundingSphere.h"
#include "../InstrumentorTimer.h"

namespace DSM {
    ShadowPass::ShadowPass(Renderer &renderer, ShadowSetting shadowSetting)
    {
        sm_Setting = shadowSetting;
        auto device = renderer.GetDevice();

        m_CmdList = device->CreateCommandList(CommandListParameters().SetDebugName("Shadow Pass Command List"));

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
    
    void ShadowPass::Render(Renderer &renderer, float deltaTime)
    {
        m_DirectionalLights.clear();
        for(const auto& light : g_RenderResources.lights){
            if(light.lightType == LightType::Directional){
                m_DirectionalLights.push_back(light);
            }
        }

        auto dirLightCount = std::min(m_DirectionalLights.size(), sm_MaxShadowedDirectionalLightCount);

        size_t tiles = dirLightCount * sm_Setting.directionalSetting.cascadeCount;
        // 拆分为多少块
        size_t split = tiles <= 1 ? 1 : (tiles <= 4 ? 2 : 4);
        size_t tileSize = sm_Setting.directionalSetting.size / split;

        // 计算包围球
        auto view = DSMEngine::sm_GlobalContext.scene->GetAllObjectsWithComponents<Model, Math::Transform>();
        // 计算所有模型的包围盒
        Math::AxisAlignedBox boundingBox{};
        for (const auto& [entity, model, transform] : view.each()) {
            auto transBox = model.boundingBox * transform;
            boundingBox = Math::AxisAlignedBox::Union(boundingBox, transBox);
        }
        Math::BoundingSphere boundingSphere{boundingBox};
        if(boundingSphere.GetRadius() <= 0.f)
            return;
        
        // 获取相机的视锥体
        const auto& camera = renderer.GetCamera();
        m_CameraFrustum = Math::Frustum{camera.GetProjMatrix()};

        // 变换到世界空间
        m_CameraFrustum *= Math::Matrix4::Inverse(camera.GetViewMatrix());

        auto forwordDir = camera.GetLookAxis();
        auto cameraSphereDir = camera.GetPosition() - boundingSphere.GetCenter();
        auto cameraLen = Math::Vector3::Dot(forwordDir, cameraSphereDir) / forwordDir.Magnitude();
        cameraLen = boundingSphere.GetRadius() - cameraLen;
        if(cameraLen > camera.GetNearZ()){
            cameraLen = std::min(float(cameraLen), camera.GetFarZ());
            m_CameraFrustum.SetFarPlane(cameraLen);
        }

        m_CmdList->Open();

        // 开始计时
        m_CmdList->BeginTimerQuery(sm_TimerQuery);

        m_CmdList->ClearDepthStencilTexture(m_ShadowMap, AllSubresources, true, 1, false, 0);

        for(size_t i = 0; i < dirLightCount; ++i){
            RenderDirectionalShadow(renderer, boundingSphere, i, split, tileSize);
        }
        // 转换为着色器资源以供后续 Pass 使用
        m_CmdList->SetTextureState(m_ShadowMap, AllSubresources, ResourceStates::PixelShaderResource);

        float zRange = m_CameraFrustum.GetFarPlane() - m_CameraFrustum.GetNearPlane();
        Math::Vector3 cascadeRatios = sm_Setting.directionalSetting.cascadeRatio;
        uint32_t cascadeCount = sm_Setting.directionalSetting.cascadeCount;

        // 写入阴影变换矩阵
        ShadowConstants shadowConstants{};
        for(size_t i = 0; i < dirLightCount * cascadeCount; ++i){
            shadowConstants.shadowViewProjs[i] = m_DirectionalShadowMatrices[i];
        }
        shadowConstants.recMaxDistance = 1.0f / sm_Setting.distance;
        shadowConstants.recDistanceFade = 1.0f / sm_Setting.distanceFade;
        auto cascadeFade = sm_Setting.directionalSetting.cascadeFace;
        shadowConstants.cascadeFade = 1.f / (1 - cascadeFade * cascadeFade);
        shadowConstants.cascadeCount = cascadeCount;
        for(size_t i = 0; i < shadowConstants.cascadeCount; ++i) {
            float distance = (i == shadowConstants.cascadeCount - 1) ?
                m_CameraFrustum.GetFarPlane() : float(m_CameraFrustum.GetNearPlane() + zRange * cascadeRatios.Get(i));
            shadowConstants.cascadeFarPlaneDist.Set(i, distance);
        }
        m_CmdList->WriteBuffer(m_ShadowCB, &shadowConstants, sizeof(ShadowConstants));

        // 结束计时
        m_CmdList->EndTimerQuery(sm_TimerQuery);

        m_CmdList->Close();
        renderer.GetDevice()->ExecuteCommandList(m_CmdList);
    }

    void ShadowPass::RenderDirectionalShadow(Renderer &renderer, const Math::BoundingSphere& boundingSphere, size_t index, size_t split, size_t tileSize)
    {
        auto device = renderer.GetDevice();
        
        const Light& dirLight = m_DirectionalLights[index];
        DrawShadowConstants shadowCB{};

        Camera lightCamera{};
        float radius = boundingSphere.GetRadius();
        auto lightPos = -dirLight.direction * 2 * radius;
        Math::Vector4 center{boundingSphere.GetCenter(), 1};
        lightCamera.SetPosition(Math::Vector3{center} + lightPos);
        lightCamera.LookAt(Math::Vector3{center}, {0,1,0});

        auto lightView = lightCamera.GetViewMatrix();
        center = center * lightView;

        size_t cascadeCount = sm_Setting.directionalSetting.cascadeCount;
        size_t tileOffset = index * cascadeCount;
        Math::Vector3 ratios = sm_Setting.directionalSetting.cascadeRatio;

        for(size_t i = 0; i < cascadeCount; ++i){
            // 计算级联的视锥体
            auto cascadeFrustum = m_CameraFrustum;
            float zRange = cascadeFrustum.GetFarPlane() - cascadeFrustum.GetNearPlane();
            float nearPlane = cascadeFrustum.GetNearPlane();
            if(i != 0){
                nearPlane = cascadeFrustum.GetNearPlane() + zRange * ratios.Get(i - 1);
            }
            float farPlane = cascadeFrustum.GetFarPlane();
            if(i != cascadeCount - 1){
                farPlane = cascadeFrustum.GetNearPlane() + zRange * ratios.Get(i);
            }

            cascadeFrustum.SetNearPlane(nearPlane);
            cascadeFrustum.SetFarPlane(farPlane);
            
            // 将视锥体变换到光照空间
            cascadeFrustum *= lightView;
            auto frustumCorners = cascadeFrustum.GetCorners();
            Math::AxisAlignedBox cameraBounds{std::span{frustumCorners}};
            Math::Vector3 lightCameraMin = cameraBounds.GetMin();
            Math::Vector3 lightCameraMax = cameraBounds.GetMax();
            
            Math::Vector3 worldUnitsPerTexelVec = (lightCameraMax - lightCameraMin) / float(tileSize);
            lightCameraMin /= worldUnitsPerTexelVec;
            lightCameraMin = Math::Vector3::Floor(lightCameraMin);
            lightCameraMin *= worldUnitsPerTexelVec;

            lightCameraMax /= worldUnitsPerTexelVec;
            lightCameraMax = Math::Vector3::Floor(lightCameraMax);
            lightCameraMax *= worldUnitsPerTexelVec;

            size_t tileIndex = tileOffset + i;

            // 获取正交投影
            float l = std::max(center.Get(0) - radius, lightCameraMin.Get(0));
            float b = std::max(center.Get(1) - radius, lightCameraMin.Get(1));
            float n = std::max(center.Get(2) - radius, lightCameraMin.Get(2));
            float r = std::min(center.Get(0) + radius, lightCameraMax.Get(0));
            float t = std::min(center.Get(1) + radius, lightCameraMax.Get(1));
            float f = std::min(center.Get(2) + radius, lightCameraMax.Get(2));
            Math::Matrix4 proj = Math::GetOrthographicMatrix(l, r, b, t, n, f);

            shadowCB.viewProj = Math::Matrix4::Transpose(lightView * proj);
            Math::Vector2 offset{float(tileIndex % split), float(tileIndex / split)};
            m_DirectionalShadowMatrices[tileIndex] = ConvertToAtlasMatrix(shadowCB.viewProj, offset, 1.f / split);

            DrawModelShadow(device, shadowCB, GetTileViewport(tileIndex, split, tileSize));
        }
    }
    
    void ShadowPass::DrawModelShadow(IDevice *device, DrawShadowConstants &shadowCB, Viewport viewport)
    {
        auto view = DSMEngine::sm_GlobalContext.scene->GetAllObjectsWithComponents<Model, Math::Transform>();
        for(const auto& [entity, model, transform] : view.each()) {
            auto bufferSize = Math::Align(sizeof(DrawShadowConstants), size_t(c_ConstantBufferOffsetSizeAlignment));
            BufferHandle shadowConstantsBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(bufferSize * model.materials.size())
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("Shadow Pass CB"));
            std::vector<bool> writtenMaterials(model.materials.size(), false);

            MeshConstants meshCB{};
            meshCB.world = Math::Matrix4::Transpose(transform.GetLocalToWorld());
            meshCB.worldIT = Math::Matrix4::Inverse(meshCB.world);
            auto meshConstantBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(MeshConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("Mesh CB"));
            m_CmdList->WriteBuffer(meshConstantBuffer, &meshCB, sizeof(MeshConstants));

            // 减少 BindingSet 的复杂度
            auto commonBindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem().ConstantBuffer(0, meshConstantBuffer))
                .AddItem(BindingSetItem().Sampler(uint32_t(SamplerSlot::AnisoWrap), GetCommonSampler(SamplerSlot::AnisoWrap))),
                m_ShadowBindingLayouts[0]);

            for(const auto& mesh : model.meshes){
                bool alphaClip = HasFlags(PSOFlags(mesh->psoFlags), PSOFlags::kAlphaBlend);
                auto bufferOffset = bufferSize * mesh->materialIndex;
                // 避免重复写入
                if(!writtenMaterials[mesh->materialIndex]){
                    shadowCB.baseColor = model.materials[mesh->materialIndex]->baseColor;
                    m_CmdList->WriteBuffer(shadowConstantsBuffer, &shadowCB, sizeof(DrawShadowConstants), bufferOffset);
                    writtenMaterials[mesh->materialIndex] = true;
                }

                size_t baseIndex = alphaClip ? 0 : (ShadowSetting::FilterMode::_PCF7x7  +1);
                const auto& pipeline = m_ShadowPipelineDescs[baseIndex + sm_Setting.directionalSetting.filter];

                auto bindingSet = device->CreateBindingSet(BindingSetDesc()
                    .AddItem(BindingSetItem().ConstantBuffer(1, shadowConstantsBuffer, 
                        BufferRange{}.SetByteOffset(bufferOffset).SetByteSize(bufferSize)))
                    .AddItem(BindingSetItem().Texture_SRV(0, mesh->textures[kBaseColor]))
                    , m_ShadowBindingLayouts[1]);

                GraphicsState state{};
                state.SetFramebuffer(m_ShadowFramebuffer)
                    .SetPipeline(pipeline)
                    .SetViewport(ViewportState().AddViewportAndScissorRect(viewport))
                    .SetIndexBuffer(mesh->indexBufferViews)
                    .AddVertexBuffer(mesh->positionStream)
                    .AddVertexBuffer(mesh->uvStream)
                    .AddBindingSet(commonBindingSet, 0)
                    .AddBindingSet(bindingSet, 1);

                m_CmdList->SetGraphicsState(state);

                m_CmdList->DrawIndexed(DrawArguments()
                    .SetVertexCount(mesh->indexCount)
                    .SetStartIndexLocation(mesh->indexOffset)
                    .SetStartVertexLocation(mesh->vertexOffset));
            }
        }
    }

    Viewport ShadowPass::GetTileViewport(size_t index, size_t split, size_t tileSize) const
    {
        size_t x = (index % split) * tileSize;
        size_t y = (index / split) * tileSize;
        return Viewport(x, x + tileSize, y, y + tileSize, 0, 1);
    }

    Math::Matrix4 ShadowPass::ConvertToAtlasMatrix(const Math::Matrix4 &m, Math::Vector2 offset, float scale) const
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
}
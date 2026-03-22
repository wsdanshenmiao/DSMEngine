#include "ShadowPass.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Render/Model.h"
#include "Shaders/ForwardShader/ResourceData.h"
#include "Runtime/Math/Collision/BoundingSphere.h"
#include "Runtime/Core/InstrumentorTimer.h"

namespace DSM {
    ShadowPass::ShadowPass(Renderer &renderer, ShadowSetting shadowSetting)
    {
        sm_Setting = shadowSetting;
        auto device = renderer.GetDevice();

        m_CmdList = device->CreateCommandList(CommandListParameters().SetDebugName("Shadow Pass Command List"));

        sm_ShadowCB = device->CreateBuffer(BufferDesc()
            .SetByteSize(sizeof(ShaderResource::ShadowConstants))
            .SetIsConstantBuffer(true)
            .SetDebugName("Shadow Constants Buffer"));

        auto passCBSize = Math::Align(sizeof(Math::Matrix4), (size_t)c_ConstantBufferOffsetSizeAlignment);
        m_PassCB = device->CreateBuffer(BufferDesc()
            .SetByteSize(passCBSize * sm_Setting.sm_MaxCascadeCount)
            .SetIsConstantBuffer(true)
            .SetIsVolatile(true)
            .SetDebugName("Shadow Pass PassConstants Buffer"));

        ResizeShadowMap(device);

        // 编译 ShadowPass 的着色器
        auto createShader = [&](ShaderType type, const std::string& entryPoint, const auto& define) {
            auto desc = ShaderCompileDesc()
                .SetType(type)
                .SetMode(ShaderMode::SM_6_6)
                .SetEnterPoint(entryPoint)
                .SetFilename("Shaders/ForwardShader/Passes/ShadowPass.hlsl");
            if(!std::empty(define)){
                desc.AddDefine(define, "1");
            }
            ShaderByteCode byteCode{desc};
            return device->CreateShader(ShaderDesc()
                .SetEntryName(desc.enterPoint)
                .SetShaderType(desc.type)
                .SetDebugName(desc.enterPoint), 
                byteCode.GetByteCode(), byteCode.GetByteCodeSize());
        };
        std::array<ShaderHandle, ShaderSlot::Count> shaders{};
        shaders[ShaderSlot::ShadowVS] = createShader(ShaderType::Vertex, "ShadowPassVS", std::string{});
        shaders[ShaderSlot::ShadowVSClip] = createShader(ShaderType::Vertex, "ShadowPassVS", "SHADOWS_CLIP");
        shaders[ShaderSlot::ShadowPS] = createShader(ShaderType::Pixel, "ShadowPassPS", std::string{});
        shaders[ShaderSlot::ShadowPSClip] = createShader(ShaderType::Pixel, "ShadowPassPS", "SHADOWS_CLIP");

        m_ShadowBindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
            .AddItem(BindingLayoutItem().VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem().PushConstants(1, sizeof(int)))
            .AddItem(BindingLayoutItem().Sampler(uint32_t(SamplerSlot::AnisoWrap)))
            .AddItem(BindingLayoutItem().StructuredBuffer_SRV(0))
            .AddItem(BindingLayoutItem().StructuredBuffer_SRV(1))
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
                .AddBindingLayout(m_ShadowBindingLayout, 0)
                .AddBindingLayout(g_RenderResources.textureBindlessLayout, 1);
            auto pso = device->CreateGraphicsPipeline(pipelineDesc, m_ShadowFramebuffer);
            return pso;
        };
        size_t count = ShadowSetting::FilterMode::Count;
        for(size_t i = 0; i < 2 * count; ++i){
            bool clip = i < count;
            ShaderHandle vs = shaders[clip ? size_t(ShaderSlot::ShadowVSClip) : size_t(ShaderSlot::ShadowVS)];
            ShaderHandle ps = shaders[clip ? size_t(ShaderSlot::ShadowPSClip) : size_t(ShaderSlot::ShadowPS)];
            auto renderState = RenderState{};
            float scale = (i % count) + 1;
            renderState.rasterState.SetSlopeScaleDepthBias(1.5f * scale).SetDepthBias(100 * scale);
            auto pso = createPipeline(vs, ps, clip, renderState);
            m_ShadowPipeline.push_back(pso);
        }

        sm_TimerQuery = device->CreateTimerQuery();
    }

    ShadowPass::~ShadowPass()
    {
        sm_ShadowCB = nullptr;
    }

    void ShadowPass::Render(Renderer &renderer, float deltaTime)
    {
        // 检测是否要调整阴影图大小
        const auto& shadowMapDesc = g_RenderResources.commonTextures[static_cast<size_t>(CommonTextureSlot::ShadowMap)]->GetDesc();
        if(sm_Setting.directionalSetting.size != shadowMapDesc.width || sm_Setting.directionalSetting.size != shadowMapDesc.height){
            ResizeShadowMap(renderer.GetDevice());
        }

        // 从全局资源中获取所有的定向光
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
        auto view = DSMEngine::sm_GlobalContext.scene->GetObjectsWithComponents<Math::AxisAlignedBox, Math::Transform>();
        // 计算所有模型的包围盒
        Math::AxisAlignedBox boundingBox{};
        for (const auto& [entity, bounds, transform] : view.each()) {
            auto transBox = bounds * transform;
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

        m_CmdList->ClearDepthStencilTexture(GetCommonTexture(CommonTextureSlot::ShadowMap), AllSubresources, true, 1, false, 0);

        for(size_t i = 0; i < dirLightCount; ++i){
            RenderDirectionalShadow(renderer, boundingSphere, i, split, tileSize);
        }
        // 转换为着色器资源以供后续 Pass 使用
        m_CmdList->SetTextureState(GetCommonTexture(CommonTextureSlot::ShadowMap), AllSubresources, ResourceStates::PixelShaderResource);

        float zRange = m_CameraFrustum.GetFarPlane() - m_CameraFrustum.GetNearPlane();
        Math::Vector3 cascadeRatios = sm_Setting.directionalSetting.cascadeRatio;
        uint32_t cascadeCount = sm_Setting.directionalSetting.cascadeCount;

        // 写入阴影变换矩阵
        ShaderResource::ShadowConstants shadowConstants{};
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
        m_CmdList->WriteBuffer(sm_ShadowCB, &shadowConstants, sizeof(ShaderResource::ShadowConstants));

        // 结束计时
        m_CmdList->EndTimerQuery(sm_TimerQuery);

        m_CmdList->Close();
        renderer.GetDevice()->ExecuteCommandList(m_CmdList);
    }

    void ShadowPass::RenderDirectionalShadow(Renderer &renderer, const Math::BoundingSphere& boundingSphere, size_t index, size_t split, size_t tileSize)
    {
        auto device = renderer.GetDevice();
        
        const Light& dirLight = m_DirectionalLights[index];

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

        for(size_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex){
            // 计算级联的视锥体
            auto cascadeFrustum = m_CameraFrustum;
            float zRange = cascadeFrustum.GetFarPlane() - cascadeFrustum.GetNearPlane();
            float nearPlane = cascadeFrustum.GetNearPlane();
            if(cascadeIndex != 0){
                nearPlane = cascadeFrustum.GetNearPlane() + zRange * ratios.Get(cascadeIndex - 1);
            }
            float farPlane = cascadeFrustum.GetFarPlane();
            if(cascadeIndex != cascadeCount - 1){
                farPlane = cascadeFrustum.GetNearPlane() + zRange * ratios.Get(cascadeIndex);
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

            size_t tileIndex = tileOffset + cascadeIndex;

            // 获取正交投影
            float l = std::max(center.Get(0) - radius, lightCameraMin.Get(0));
            float b = std::max(center.Get(1) - radius, lightCameraMin.Get(1));
            float n = std::max(center.Get(2) - radius, lightCameraMin.Get(2));
            float r = std::min(center.Get(0) + radius, lightCameraMax.Get(0));
            float t = std::min(center.Get(1) + radius, lightCameraMax.Get(1));
            float f = std::min(center.Get(2) + radius, lightCameraMax.Get(2));
            Math::Matrix4 proj = Math::GetOrthographicMatrix(l, r, b, t, n, f);

            auto viewProj = Math::Matrix4::Transpose(lightView * proj);
            Math::Vector2 offset{float(tileIndex % split), float(tileIndex / split)};
            m_DirectionalShadowMatrices[tileIndex] = ConvertToAtlasMatrix(viewProj, offset, 1.f / split);

            DrawModelShadow(device, viewProj, GetTileViewport(tileIndex, split, tileSize), cascadeIndex);
        }
    }
    
    void ShadowPass::DrawModelShadow(IDevice *device, const Math::Matrix4 &viewProj, Viewport viewport, size_t cascadeIndex)
    {
        auto passCBOffset = cascadeIndex * Math::Align(sizeof(Math::Matrix4), (size_t)c_ConstantBufferOffsetSizeAlignment);
        m_CmdList->WriteBuffer(m_PassCB, &viewProj, sizeof(Math::Matrix4), passCBOffset);
        auto passCBRange = BufferRange{}.SetByteOffset(passCBOffset).SetByteSize(sizeof(Math::Matrix4));
        size_t sampleIndex = size_t(SamplerSlot::AnisoWrap);
        auto bindingSet = device->CreateBindingSet(BindingSetDesc()
            .AddItem(BindingSetItem().ConstantBuffer(0, m_PassCB, passCBRange))
            .AddItem(BindingSetItem().StructuredBuffer_SRV(0, g_RenderResources.meshBuffer))
            .AddItem(BindingSetItem().StructuredBuffer_SRV(1, g_RenderResources.materialBuffer))
            .AddItem(BindingSetItem().Sampler(sampleIndex, GetCommonSampler(sampleIndex)))
            , m_ShadowBindingLayout);

        for(const auto& [index, obj] : g_RenderResources.objects | std::views::enumerate){
            auto [mesh, material, transfrom] = obj->GetComponent<Mesh, ShaderResource::MaterialData, Math::Transform>();

            bool alphaClip = HasFlags(PSOFlags(mesh->psoFlags), PSOFlags::kAlphaBlend);
            size_t baseIndex = alphaClip ? 0 : ShadowSetting::FilterMode::Count;
            const auto& pipeline = m_ShadowPipeline[baseIndex + sm_Setting.directionalSetting.filter];

            GraphicsState state{};
            state.SetFramebuffer(m_ShadowFramebuffer)
                .SetPipeline(pipeline)
                .SetViewport(ViewportState().AddViewportAndScissorRect(viewport))
                .SetIndexBuffer(mesh->indexBufferViews)
                .AddVertexBuffer(mesh->positionStream)
                .AddVertexBuffer(mesh->uvStream)
                .AddBindingSet(bindingSet, 0)
                .AddBindingSet(g_RenderResources.textureBindlessTable, 1);

            m_CmdList->SetGraphicsState(state);

            int objectIndex = int(index);
            m_CmdList->SetPushConstants(&objectIndex, sizeof(int));

            m_CmdList->DrawIndexed(DrawArguments()
                .SetVertexCount(mesh->indexCount)
                .SetStartIndexLocation(mesh->indexOffset)
                .SetStartVertexLocation(mesh->vertexOffset));
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
    
    void ShadowPass::ResizeShadowMap(IDevice* device)
    {
        DSM_ASSERT(device != nullptr, "Device is null");

        auto& shadowMapTex = g_RenderResources.commonTextures[static_cast<size_t>(CommonTextureSlot::ShadowMap)];
        shadowMapTex = device->CreateTexture(TextureDesc()
            .SetWidth(sm_Setting.directionalSetting.size)
            .SetHeight(sm_Setting.directionalSetting.size)
            .SetClearValue(Color{1,1,1,1})
            .SetFormat(Format::D32)
            .SetIsRenderTarget(true)
            .SetDebugName("ShadowMap"));

        m_ShadowFramebuffer = device->CreateFramebuffer(FramebufferDesc().SetDepthAttachment(shadowMapTex));
    }
}
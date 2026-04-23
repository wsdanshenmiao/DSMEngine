#include "Shadows.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Math/Collision/BoundingSphere.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include "Shaders/ForwardShader/ResourceData.h"

namespace DSM {
    Shadows::Shadows(GraphicsRenderer &renderer, ShadowSetting shadowSetting)
    {
        sm_Setting = shadowSetting;
        auto device = renderer.GetDevice();

        m_CmdList = device->CreateCommandList(CommandListParameters().SetDebugName("Shadow Pass Command List"));

        sm_ShadowCB = device->CreateBuffer(BufferDesc()
            .SetByteSize(sizeof(ShaderResource::ShadowConstants))
            .SetIsConstantBuffer(true)
            .SetDebugName("Shadow Constants Buffer"));
        sm_DirectionalShadowMatrixBuffer = device->CreateBuffer(BufferDesc()
            .SetByteSize(sizeof(Math::Matrix4) * sm_MaxShadowedDirectionalLightCount * ShadowSetting::sm_MaxCascadeCount)
            .SetStructStride(sizeof(Math::Matrix4))
            .SetDebugName("Directional Shadow Matrices Buffer"));
        sm_OtherLightShadowDataBuffer = device->CreateBuffer(BufferDesc()
            .SetByteSize(sizeof(ShaderResource::OtherLightShadowData) * sm_MaxShadowedOtherLightCount)
            .SetStructStride(sizeof(ShaderResource::OtherLightShadowData))
            .SetDebugName("Other Shadow Matrices Buffer"));


        m_PassCB = device->CreateBuffer(BufferDesc()
            .SetByteSize(sizeof(Math::Matrix4))
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
        auto shadowVS = createShader(ShaderType::Vertex, "ShadowPassVS", std::string{});
        auto shadowVSClip = createShader(ShaderType::Vertex, "ShadowPassVS", "SHADOWS_CLIP");
        auto shadowPS = createShader(ShaderType::Pixel, "ShadowPassPS", std::string{});
        auto shadowPSClip = createShader(ShaderType::Pixel, "ShadowPassPS", "SHADOWS_CLIP");

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
                .SetFormat(Format::RGB32_FLOAT)
                .SetBufferIndex(0)
                .SetElementStride(sizeof(Math::Vector3)),
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
                .SetInputLayout(device->CreateInputLayout(vertexDescs, vsHandle))
                .SetRenderState(renderState)
                .AddBindingLayout(m_ShadowBindingLayout, 0);
            if(psHandle != nullptr){
                pipelineDesc.SetPixelShader(psHandle)
                    .AddBindingLayout(RenderResource::GetInstance().GetTextureBindlessLayout(), 1);
            }
            auto pso = device->CreateGraphicsPipeline(pipelineDesc, m_DirectionalShadowFB);
            return pso;
        };
        size_t filterModeCount = size_t(ShadowSetting::FilterMode::Count);
        for(size_t i = 0; i < m_ShadowPipeline.size(); ++i){
            ShadowSetting::FilterMode filterMode = ShadowSetting::FilterMode(i % filterModeCount);
            ShadowOption option = ShadowOption(i / ShadowSetting::FilterMode::Count);
            bool clip = HasFlags(option, ShadowOption::AlphaClip);
            bool enableDepthClip = HasFlags(option, ShadowOption::EnableDepthClip);
            ShaderHandle vs = clip ? shadowVSClip : shadowVS;
            ShaderHandle ps = clip ? shadowPSClip : shadowPS;
            auto renderState = RenderState{};
            float scale = (i % filterModeCount) + 1;
            // 点光源和聚光灯需要开启深度裁剪确保远平面外的物体被裁剪，平行光则需关闭，保证深度正确
            renderState.rasterState
                .SetSlopeScaleDepthBias(1.5f * scale)
                .SetDepthBias(100 * scale)
                .SetDepthClipEnable(enableDepthClip);
            auto pso = createPipeline(vs, ps, clip, renderState);
            m_ShadowPipeline[i] = pso;
        }
    }

    Shadows::~Shadows()
    {
        sm_ShadowCB = nullptr;
        sm_DirectionalShadowMatrixBuffer = nullptr;
        sm_OtherLightShadowDataBuffer = nullptr;
    }

    void Shadows::Setup()
    {
        m_ReservedDirectionalLights.clear();
        m_ReservedOtherLights.clear();
        m_OtherLightShadowCount = 0;
    }

    uint64_t Shadows::Render(GraphicsRenderer &renderer, float deltaTime)
    {
        auto& renderRes = RenderResource::GetInstance();

        // 检测是否要调整阴影图大小
        ResizeShadowMap(renderer.GetDevice());

        auto dirLightCount = std::min(m_ReservedDirectionalLights.size(), sm_MaxShadowedDirectionalLightCount);

        if(m_CacheMeshBuffer != renderRes.GetMeshBuffer() ||
              m_CacheMaterialBuffer != renderRes.GetMaterialBuffer())
        {
            m_CacheMeshBuffer = renderRes.GetMeshBuffer();
            m_CacheMaterialBuffer = renderRes.GetMaterialBuffer();

            size_t sampleIndex = size_t(SamplerSlot::AnisoWrap);
            m_ShadowBindingSet = renderer.GetDevice()->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem().ConstantBuffer(0, m_PassCB))
                .AddItem(BindingSetItem().StructuredBuffer_SRV(0, m_CacheMeshBuffer))
                .AddItem(BindingSetItem().StructuredBuffer_SRV(1, m_CacheMaterialBuffer))
                .AddItem(BindingSetItem().Sampler(sampleIndex, renderRes.GetCommonSampler(sampleIndex)))
                , m_ShadowBindingLayout);
        }

        m_CmdList->Open();

        m_CmdList->ClearDepthStencilTexture(renderRes.GetCommonTexture(CommonTextureSlot::DirectionalShadowMap), AllSubresources, true, 1, false, 0);
        m_CmdList->ClearDepthStencilTexture(renderRes.GetCommonTexture(CommonTextureSlot::OtherShadowMap), AllSubresources, true, 1, false, 0);

        RenderDirectionalShadow(m_ReservedDirectionalLights, renderer.GetCamera());
        RenderOtherShadow(m_ReservedOtherLights, renderer.GetCamera());

        // 转换为着色器资源以供后续 Pass 使用
        m_CmdList->SetTextureState(renderRes.GetCommonTexture(CommonTextureSlot::DirectionalShadowMap), AllSubresources, ResourceStates::PixelShaderResource);
        m_CmdList->SetTextureState(renderRes.GetCommonTexture(CommonTextureSlot::OtherShadowMap), AllSubresources, ResourceStates::PixelShaderResource);

        const auto& dirSetting = sm_Setting.directionalSetting;
        float zRange = m_CameraFrustum.GetFarPlane() - m_CameraFrustum.GetNearPlane();
        Math::Vector3 cascadeRatios = dirSetting.cascadeRatio;
        uint32_t cascadeCount = dirSetting.cascadeCount;

        // 写入阴影变换矩阵
        ShaderResource::ShadowConstants shadowConstants{};
        shadowConstants.directionalShadowMapSize = Math::Vector2{float(dirSetting.size), 1.f / float(dirSetting.size)};
        shadowConstants.otherShadowMapSize = Math::Vector2{float(sm_Setting.otherSetting.size), 1.f / float(sm_Setting.otherSetting.size)};
        shadowConstants.recMaxDistance = 1.0f / sm_Setting.distance;
        shadowConstants.recDistanceFade = 1.0f / sm_Setting.distanceFade;
        auto cascadeFade = 1 - dirSetting.cascadeFace;
        shadowConstants.cascadeFade = 1.f / (1 - cascadeFade * cascadeFade);
        shadowConstants.cascadeCount = cascadeCount;
        for(size_t i = 0; i < shadowConstants.cascadeCount; ++i) {
            float distance = (i == shadowConstants.cascadeCount - 1) ?
                m_CameraFrustum.GetFarPlane() : float(m_CameraFrustum.GetNearPlane() + zRange * cascadeRatios.Get(i));
            shadowConstants.cascadeFarPlaneDist.Set(i, distance);
        }

        m_CmdList->WriteBuffer(sm_ShadowCB, &shadowConstants, sizeof(ShaderResource::ShadowConstants));
        m_CmdList->WriteBuffer(sm_DirectionalShadowMatrixBuffer, 
            m_DirectionalShadowMatrices.data(), 
            sizeof(Math::Matrix4) * m_DirectionalShadowMatrices.size());
        m_CmdList->WriteBuffer(sm_OtherLightShadowDataBuffer, 
            m_OtherLightShadowData.data(), 
            sizeof(ShaderResource::OtherLightShadowData) * m_OtherLightShadowData.size());

        m_CmdList->Close();
        return renderer.GetDevice()->ExecuteCommandList(m_CmdList);
    }

    Math::Vector4 Shadows::ReserveDirectionalShadows(const Light &light)
    {
        if(m_ReservedDirectionalLights.size() >= sm_MaxShadowedDirectionalLightCount ||
            light.GetType() != LightType::Directional)
        {
            return Math::Vector4{};
        }

        float index = m_ReservedDirectionalLights.size();
        m_ReservedDirectionalLights.push_back(&light);
        return Math::Vector4{1, index * sm_Setting.directionalSetting.cascadeCount, 0, 0};
    }

    Math::Vector4 Shadows::ReserveOtherShadows(const Light &light)
    {
        auto lightType = light.GetType();
        size_t shadowCount = lightType == LightType::Point ? 6 : 1;
        if(m_OtherLightShadowCount + shadowCount > sm_MaxShadowedOtherLightCount ||
            (lightType != LightType::Point && lightType != LightType::Spot))
        {
            return Math::Vector4{};
        }

        m_ReservedOtherLights.push_back(&light);
        float index = m_OtherLightShadowCount;
        m_OtherLightShadowCount += shadowCount;
        return Math::Vector4{1, index, lightType == LightType::Point ? 1.f : 0, 0};
    }

    void Shadows::RenderDirectionalShadow(std::span<const Light *> directionalLights, const Camera &camera)
    {
        if(directionalLights.empty())
            return;

        auto& renderRes = RenderResource::GetInstance();

        auto bvhRoot = renderRes.GetBVH().GetRoot();
        if(bvhRoot == nullptr)
            return;
        Math::BoundingSphere boundingSphere{bvhRoot->bounds};
        if(boundingSphere.GetRadius() <= 0.f)
            return;

        // 获取相机的视锥体
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

        auto dirLightCount = std::min(directionalLights.size(), sm_MaxShadowedDirectionalLightCount);
        size_t tilesCount = dirLightCount * sm_Setting.directionalSetting.cascadeCount;
        // 拆分为多少块
        size_t split = tilesCount <= 1 ? 1 : (tilesCount <= 4 ? 2 : 4);
        size_t tileSize = sm_Setting.directionalSetting.size / split;
        for(size_t i = 0; i < dirLightCount; ++i){
            RenderDirectionalShadow(boundingSphere, directionalLights, i, split, tileSize);
        }
    }

    void Shadows::RenderDirectionalShadow(
        const Math::BoundingSphere &boundingSphere,
        std::span<const Light *> directionalLights,
        size_t index, size_t split, size_t tileSize)
    {
        const Light& dirLight = *directionalLights[index];

        Camera lightCamera{};
        float radius = boundingSphere.GetRadius();
        auto lightPos = -dirLight.GetDirection() * 2 * radius;
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
            if(cascadeIndex > 1){
                nearPlane = cascadeFrustum.GetNearPlane() + zRange * ratios.Get(cascadeIndex - 2);
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

            DrawModelShadow(m_DirectionalShadowFB, viewProj, GetTileViewport(tileIndex, split, tileSize), true);
        }
    }

    void Shadows::RenderOtherShadow(std::span<const Light *> otherLights, const Camera &camera)
    {
        size_t shadowedLightCount = 0;
        for(size_t lightIndex = 0; lightIndex < otherLights.size(); ++lightIndex){
            const auto& light = *otherLights[lightIndex];
            size_t currShadowCount = (light.GetType() == LightType::Point) ? 6 : 1;
            if(shadowedLightCount >= sm_MaxShadowedOtherLightCount){
                break;
            }
            else if(shadowedLightCount + currShadowCount > sm_MaxShadowedOtherLightCount){
                continue;
            }

            switch (light.GetType()) {
                case LightType::Point:
                    RenderPointLightShadow(light, shadowedLightCount);
                    shadowedLightCount += 6;
                    break;
                case LightType::Spot:
                    RenderSpotLightShadow(light, shadowedLightCount);
                    ++shadowedLightCount;
                    break;
                default:
                    break;
            }
        }
    }
    
    void Shadows::RenderPointLightShadow(const Light &light, size_t index)
    {
        Camera lightCamera{};
        lightCamera.SetPosition(light.GetPosition());

        size_t split = m_OtherLightShadowCount <= 1 ? 1 : (m_OtherLightShadowCount <= 4 ? 2 : 4);
        const size_t atlasSize = sm_Setting.otherSetting.size;
        const size_t tileSize = atlasSize / split;

        const float range = std::max(light.GetRange(), 1e-3f);
        const float nearZ = std::max(0.001f, range * 0.0001f);
        const float farZ = std::max(range, nearZ + 1e-3f);
        const float filterSize = 2.f / tileSize;
		const float fov = std::atanf(1 + filterSize) * 2;
        lightCamera.SetFrustum(fov, 1.0f, nearZ, farZ);

        for(size_t face = 0; face < 6; ++face){
            auto faceDir = GetCubeMapFaceDirection(face);
            Math::Vector3 up;
            switch(face) {
                case 0: // +X
                case 1: up = Math::Vector3{0, 1, 0}; break; // -X
                case 2: up = Math::Vector3{0, 0, -1}; break; // +Y
                case 3: up = Math::Vector3{0, 0, 1}; break; // -Y
                case 4: // +Z
                case 5: // -Z
                default: up = Math::Vector3{0, 1, 0}; break;
            }
            lightCamera.LookTo(faceDir, up);

            auto viewProj = Math::Matrix4::Transpose(lightCamera.GetViewProjMatrix());
            const size_t tileIndex = index + face;
            Math::Vector2 offset{float(tileIndex % split), float(tileIndex / split)};
            m_OtherLightShadowData[tileIndex] = GetOtherLightShadowData(
                ConvertToAtlasMatrix(viewProj, offset, 1.f / split),
                offset, 1.f / split, 1.f / atlasSize * 0.5f);
            DrawModelShadow(m_OtherShadowFB, viewProj, GetTileViewport(tileIndex, split, tileSize), false);
        }
    }

    void Shadows::RenderSpotLightShadow(const Light &light, size_t index)
    {
        Camera lightCamera{};
        lightCamera.SetPosition(light.GetPosition());

        const float range = std::max(light.GetRange(), 1e-3f);
        const float nearZ = std::max(0.001f, range * 0.0001f);
        const float farZ = std::max(range, nearZ + 1e-3f);
        const float minFov = std::numbers::pi_v<float> / 180.0f;
        const float maxFov = std::numbers::pi_v<float> - 1e-3f;
        const float fovY = std::clamp(light.GetOuterAngle() * 2.0f, minFov, maxFov);
        lightCamera.SetFrustum(fovY, 1.0f, nearZ, farZ);

        auto up = Math::Vector3{0, 1, 0};
        if(std::abs(Math::Vector3::Dot(light.GetDirection(), up)) > 0.999f){
            up = Math::Vector3{0, 0, 1};
        }
        lightCamera.LookTo(light.GetDirection(), up);
        auto viewProj = Math::Matrix4::Transpose(lightCamera.GetViewProjMatrix());

        size_t split = m_OtherLightShadowCount <= 1 ? 1 : (m_OtherLightShadowCount <= 4 ? 2 : 4);
        const size_t atlasSize = sm_Setting.otherSetting.size;
        const size_t tileSize = atlasSize / split;

        Math::Vector2 offset{float(index % split), float(index / split)};
        m_OtherLightShadowData[index] = GetOtherLightShadowData(
            ConvertToAtlasMatrix(viewProj, offset, 1.f / split),
            offset, 1.f / split, 1.f / atlasSize * 0.5f);
        DrawModelShadow(m_OtherShadowFB, viewProj, GetTileViewport(index, split, tileSize), false);
    }

    void Shadows::DrawModelShadow(IFramebuffer* framebuffer, const Math::Matrix4 &viewProj, Viewport viewport, bool isDirectionalLightShadow)
    {
        auto& renderRes = RenderResource::GetInstance();

        m_CmdList->WriteBuffer(m_PassCB, &viewProj, sizeof(Math::Matrix4));

        auto allObj = std::array{renderRes.GetOpaqueObjects(), renderRes.GetTransparentObjects()} | std::views::join;
        auto state = GraphicsState{}
            .AddBindingSet(m_ShadowBindingSet, 0)
            .SetFramebuffer(framebuffer)
            .SetViewport(ViewportState().AddViewportAndScissorRect(viewport));
        for(const auto& obj : allObj){
            auto meshRenderer = obj->GetComponent<MeshRenderer>();
            if(meshRenderer == nullptr)
                continue;

            auto mesh = meshRenderer->GetMesh();
            if(mesh == nullptr)
                continue;

            for(size_t subMeshIndex = 0; subMeshIndex < mesh->GetSubMeshCount(); ++subMeshIndex){
                auto matIndex = meshRenderer->GetMaterialIndex(subMeshIndex);
                auto material = meshRenderer->GetMaterial(matIndex);
                if(material == nullptr)
                    continue;

                size_t option = ShadowOption::None;
                if(material->IsTransparent()){
                    option |= ShadowOption::AlphaClip;
                }
                if(!isDirectionalLightShadow){
                    option |= ShadowOption::EnableDepthClip;
                }
                const auto& filterMode = isDirectionalLightShadow ? 
                    sm_Setting.directionalSetting.filter : sm_Setting.otherSetting.filter;
                state.vertexBuffers.resize(0);
                state.SetPipeline(GetShadowPipeline(option, filterMode))
                    .SetIndexBuffer(mesh->GetIndexBufferBinding(subMeshIndex));
                if(auto slot = Mesh::VertexAttributeSlot::Position; mesh->HasVertexAttribute(slot)){
                    state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                }
                if(auto slot = Mesh::VertexAttributeSlot::UV; mesh->HasVertexAttribute(slot)){
                    state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                }
                if(material->IsTransparent()){
                    state.AddBindingSet(renderRes.GetTextureBindlessTable(), 1);
                }

                m_CmdList->SetGraphicsState(state);

                int objectIndex = int(renderRes.GetObjectIndex().at(obj));
                m_CmdList->SetPushConstants(&objectIndex, sizeof(int));

                m_CmdList->DrawIndexed(DrawArguments()
                    .SetVertexCount(mesh->GetIndexCount(subMeshIndex))
                    .SetStartIndexLocation(mesh->GetIndexOffset(subMeshIndex))
                    .SetStartVertexLocation(mesh->GetVertexOffset(subMeshIndex)));
            }
        }
    }

    Viewport Shadows::GetTileViewport(size_t index, size_t split, size_t tileSize) const
    {
        size_t x = (index % split) * tileSize;
        size_t y = (index / split) * tileSize;
        return Viewport(x, x + tileSize, y, y + tileSize, 0, 1);
    }

    Math::Matrix4 Shadows::ConvertToAtlasMatrix(const Math::Matrix4 &m, Math::Vector2 offset, float scale) const
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
    
    void Shadows::ResizeShadowMap(IDevice* device)
    {
        DSM_ASSERT(device != nullptr, "Device is null");

        auto& renderRes = RenderResource::GetInstance();
        auto dirShadowMapSize = sm_Setting.directionalSetting.size;
        auto dirShadowMap = renderRes.GetCommonTexture(CommonTextureSlot::DirectionalShadowMap);
        if(dirShadowMap == nullptr || dirShadowMap->GetDesc().width != dirShadowMapSize || 
            dirShadowMap->GetDesc().height != dirShadowMapSize)
        {
            dirShadowMap = device->CreateTexture(TextureDesc()
                .SetWidth(dirShadowMapSize)
                .SetHeight(dirShadowMapSize)
                .SetClearValue(Color{1,1,1,1})
                .SetFormat(Format::D32)
                .SetIsRenderTarget(true)
                .SetDebugName("Directional Shadow Map"));
            renderRes.SetCommonTexture(CommonTextureSlot::DirectionalShadowMap, dirShadowMap);
            m_DirectionalShadowFB = device->CreateFramebuffer(FramebufferDesc().SetDepthAttachment(dirShadowMap));
        }

        auto otherShadowMapSize = sm_Setting.otherSetting.size;
        auto otherShadowMap = renderRes.GetCommonTexture(CommonTextureSlot::OtherShadowMap);
        if(otherShadowMap == nullptr || otherShadowMap->GetDesc().width != otherShadowMapSize || 
            otherShadowMap->GetDesc().height != otherShadowMapSize)
        {
            otherShadowMap = device->CreateTexture(TextureDesc()
                .SetWidth(otherShadowMapSize)
                .SetHeight(otherShadowMapSize)
                .SetClearValue(Color{1,1,1,1})
                .SetFormat(Format::D32)
                .SetIsRenderTarget(true)
                .SetDebugName("Other Shadow Map"));
            renderRes.SetCommonTexture(CommonTextureSlot::OtherShadowMap, otherShadowMap);
            m_OtherShadowFB = device->CreateFramebuffer(FramebufferDesc().SetDepthAttachment(otherShadowMap));
        }
    }
    
    size_t Shadows::GetCubeMapFaceIndex(const Math::Vector3 &direction) const
    {
        Math::Vector3 absDir = Math::Vector3::Abs(direction);
        size_t faceIndex = 0;
        if(absDir.Get(0) > absDir.Get(1) && absDir.Get(0) > absDir.Get(2)){
            faceIndex = (direction.Get(0) > 0.f) ? 0 : 1; // +X : -X
        }
        else if(absDir.Get(1) > absDir.Get(0) && absDir.Get(1) > absDir.Get(2)){
            faceIndex = (direction.Get(1) > 0.f) ? 2 : 3; // +Y : -Y
        }
        else{
            faceIndex = (direction.Get(2) > 0.f) ? 4 : 5; // +Z : -Z
        }
        return faceIndex;
    }
    
    Math::Vector3 Shadows::GetCubeMapFaceDirection(size_t faceIndex) const
    {
        switch(faceIndex)
        {
        case 0: return Math::Vector3{1.f, 0.f, 0.f}; // +X
        case 1: return Math::Vector3{-1.f, 0.f, 0.f}; // -X
        case 2: return Math::Vector3{0.f, 1.f, 0.f}; // +Y
        case 3: return Math::Vector3{0.f, -1.f, 0.f}; // -Y
        case 4: return Math::Vector3{0.f, 0.f, 1.f}; // +Z
        case 5: return Math::Vector3{0.f, 0.f, -1.f}; // -Z
        default: return Math::Vector3();
        }
    }

    ShaderResource::OtherLightShadowData Shadows::GetOtherLightShadowData(
        Math::Matrix4 viewProj,
        const Math::Vector2 &offset, 
        float scale, 
        float border) const
    {
        ShaderResource::OtherLightShadowData data;
        data.shadowMatrix = std::move(viewProj);
        data.shadowParams = Math::Vector4{
            offset.Get(0) * scale + border,
            offset.Get(1) * scale + border,
            scale - 2 * border,
            0};
        return data;
    }
}
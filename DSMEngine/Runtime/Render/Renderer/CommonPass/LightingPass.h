#pragma once
#ifndef __LIGHTINGPASS_H__
#define __LIGHTINGPASS_H__

#include "RenderResource.h"
#include "Shadows.h"
#include "Runtime/Math/MathCommon.h"
#include "Shaders/Common/ResourceData.h"

#include <algorithm>

namespace DSM {
    class LightingPass : public IRenderPass
    {
    public:
        struct LightSettings
        {
            enum TileSize
            {
                _8 = 8,
                _16 = 16,
                _32 = 32
            };

            TileSize tileSize = TileSize::_16;
        };

        LightingPass(GraphicsRenderer& renderer)
            : m_Shadows(std::make_unique<Shadows>(renderer))
        {
            auto device = renderer.GetDevice();
            sm_LightDataBuffer = device->CreateBuffer(BufferDesc()
                .SetInitialState(ResourceStates::ShaderResource)
                .SetByteSize(sizeof(ShaderResource::LightData))
                .SetDebugName("Light Data Buffer")
                .SetKeepInitialState(true)
                .SetIsConstantBuffer(true));
            sm_DirLightDataBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(sm_MaxDirLightCount * sizeof(ShaderResource::DirectionalLightData))
                .SetStructStride(sizeof(ShaderResource::DirectionalLightData))
                .SetInitialState(ResourceStates::ShaderResource)
                .SetDebugName("Directional Light Data Buffer")
                .SetKeepInitialState(true));
            sm_OtherLightDataBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(sm_MaxOtherLightCount * sizeof(ShaderResource::OtherLightData))
                .SetStructStride(sizeof(ShaderResource::OtherLightData))
                .SetInitialState(ResourceStates::ShaderResource)
                .SetDebugName("Other Light Data Buffer")
                .SetKeepInitialState(true));

            m_TileBasedLightCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(ShaderResource::TileBasedLightingConstants))
                .SetDebugName("Tile Based Lighting Constants Buffer")
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true));
            m_LightBoundsBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(sm_MaxOtherLightCount * sizeof(Math::AxisAlignedBox))
                .SetStructStride(sizeof(Math::AxisAlignedBox))
                .SetDebugName("Light Bounds Buffer"));
            
            m_BindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem::StructuredBuffer_UAV(0))
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(0))
                .AddItem(BindingLayoutItem::Texture_SRV(1))
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(0)));

            for(size_t i = 0; i < 3; i++){
                ShaderByteCode tileBasedLightShaderBytes{ShaderCompileDesc()
                    .SetType(ShaderType::Compute)
                    .SetEnterPoint("TileBasedLightingCS")
                    .SetMode(ShaderMode::SM_6_6)
                    .AddDefine("TILE_SIZE", std::to_string(1 << (i + 3)))
                    .SetFilename("Shaders/ForwardShader/TileBasedLighting.hlsl")};
                auto tileBasedLightShader = renderer.GetDevice()->CreateShader(ShaderDesc()
                    .SetEntryName("TileBasedLightingCS")
                    .SetShaderType(ShaderType::Compute)
                    .SetDebugName("Tile Based Light Culling Shader"),
                    tileBasedLightShaderBytes.GetByteCode(), tileBasedLightShaderBytes.GetByteCodeSize());
                m_TileBasedLightPipelines[i] = device->CreateComputePipeline(ComputePipelineDesc()
                    .SetComputeShader(tileBasedLightShader)
                    .AddBindingLayout(m_BindingLayout, 0));
            }
        }

        virtual ~LightingPass()
        {
            sm_LightDataBuffer = nullptr;
            sm_DirLightDataBuffer = nullptr;
            sm_OtherLightDataBuffer = nullptr;
            sm_TileInfoBuffer = nullptr;
        }

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            auto lights = DSMEngine::sm_GlobalContext.scene->GetObjectsWithComponents<Light>();
            const auto& camera = renderer.GetCamera();
            auto device = renderer.GetDevice();

            m_Shadows->Setup();

            size_t directionalLightCount = 0;
            size_t otherLightCount = 0;
            std::array<ShaderResource::DirectionalLightData, sm_MaxDirLightCount> dirLightData{};
            std::array<ShaderResource::OtherLightData, sm_MaxOtherLightCount> otherLightData{};
            std::array<Math::AxisAlignedBox, sm_MaxOtherLightCount> lightBounds{};

            auto cameraFrustum = camera.GetFrustum();
            for (const auto& [id, light] : lights.each()) {
                auto bounds = light.GetBounds();
                // 将光源包围盒变换到视图空间
                bounds *= camera.GetViewMatrix();
                if(!cameraFrustum.Intersects(bounds))
                    continue;

                switch(light.GetType()){
                case LightType::Directional:
                    dirLightData[directionalLightCount++] = CreateDirLightData(light, m_Shadows->ReserveDirectionalShadows(light));
                    break;
                case LightType::Point:{
                    if(otherLightCount < sm_MaxOtherLightCount) {
                        lightBounds[otherLightCount] = bounds;
                        otherLightData[otherLightCount++] = CreatePointLightData(light, m_Shadows->ReserveOtherShadows(light));
                    }
                    break;
                }
                case LightType::Spot:
                    if(otherLightCount < sm_MaxOtherLightCount) {
                        lightBounds[otherLightCount] = bounds;
                        otherLightData[otherLightCount++] = CreateSpotLightData(light, m_Shadows->ReserveOtherShadows(light));
                    }
                    break;
                default:
                    DSM_ERROR("Error light type.");
                    return 0;
                }
            }

            m_Shadows->Render(renderer, deltaTime);

            auto cmdList = device->CreateCommandList(CommandListParameters()
                .SetQueueType(CommandQueueType::Compute)
                .SetDebugName("Lighting Pass Command List"));
            cmdList->Open();

            // 写入光照数据
            ShaderResource::LightData lightData;
            lightData.dirLightCount = std::min(directionalLightCount, sm_MaxDirLightCount);
            lightData.otherLightCount = std::min(otherLightCount, sm_MaxOtherLightCount);
            cmdList->WriteBuffer(sm_LightDataBuffer, &lightData, sizeof(lightData));
            cmdList->WriteBuffer(sm_DirLightDataBuffer, dirLightData.data(), lightData.dirLightCount * sizeof(ShaderResource::DirectionalLightData));
            cmdList->WriteBuffer(sm_OtherLightDataBuffer, otherLightData.data(), lightData.otherLightCount * sizeof(ShaderResource::OtherLightData));


            // 进行基于瓦片的光照计算
            auto width = camera.GetViewPort().Width();
            auto height = camera.GetViewPort().Height();
            size_t tileCountX = Math::DivideByMultiple(width, sm_Settings.tileSize);
            size_t tileCountY = Math::DivideByMultiple(height, sm_Settings.tileSize);
            if(tileCountX == 0 || tileCountY == 0)
                return 0;
            
            size_t tileInfoSize = sizeof(ShaderResource::TileInfo);
            if(auto tileCount = tileCountX * tileCountY; sm_TileInfoBuffer == nullptr || 
                sm_TileInfoBuffer->GetDesc().byteSize < tileCount * tileInfoSize)
            {
                sm_TileInfoBuffer = device->CreateBuffer(BufferDesc()
                    .SetInitialState(ResourceStates::UnorderedAccess)
                    .SetByteSize(tileCount * tileInfoSize)
                    .SetDebugName("Tile Info Buffer")
                    .SetStructStride(tileInfoSize)
                    .SetKeepInitialState(true)
                    .SetCanHaveUAVs(true));
                CreateBindingSet(device);
            }
            
            ShaderResource::TileBasedLightingConstants tileBasedLightCBData{};
            tileBasedLightCBData.lightCount = lightData.otherLightCount;
            tileBasedLightCBData.proj = Math::Matrix4::Transpose(camera.GetProjMatrix());
            tileBasedLightCBData.screenSizeAndCameraNearFar = Math::Vector4{width, height, camera.GetNearZ(), camera.GetFarZ()};
            cmdList->WriteBuffer(m_TileBasedLightCB, &tileBasedLightCBData, sizeof(tileBasedLightCBData));
            cmdList->WriteBuffer(m_LightBoundsBuffer, lightBounds.data(), lightData.otherLightCount * sizeof(Math::AxisAlignedBox));

            size_t baseIndex = std::countr_zero(uint32_t(LightSettings::TileSize::_8));
            cmdList->SetComputeState(ComputeState{}
                .AddBindingSet(m_BindingSet)
                .SetPipeline(m_TileBasedLightPipelines[std::countr_zero(uint32_t(sm_Settings.tileSize)) - baseIndex]));
            cmdList->Dispatch(tileCountX, tileCountY);

            cmdList->Close();
            return device->ExecuteCommandList(cmdList);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            m_Shadows->OnResize(renderer, width, height);
            CreateBindingSet(renderer.GetDevice());
        }

        void SetLightSettings(const LightSettings& settings) { sm_Settings = settings; }

    private:
        /// @brief 
        /// @param light 
        /// @param shadowData x 分量储存了阴影强度，若小于等于 0 则不进行阴影计算，
        ///                 y 分量存储了该光源在阴影图中的起始索引，要获得实际的索引还需要加上级联的索引
        /// @return 
        ShaderResource::DirectionalLightData CreateDirLightData(const Light& light, Math::Vector4 shadowData)
        {
            ShaderResource::DirectionalLightData data;
            data.color = light.GetColor();
            data.direction = -Math::Vector4(light.GetDirection()).Normalized();
            data.shadowData = std::move(shadowData);
            return data;
        }

        /// @brief 
        /// @param light
        /// @param shadowData x 分量储存了阴影强度，若小于等于 0 则不进行阴影计算，
        ///                 y 分量存储了该光源在阴影图中的起始索引，还需要加上面向的立方体贴图面索引
        ///                 z 分量存储了该光源是否为点光源，若为 1 则是点光源，否则为聚光灯
        /// @return
        ShaderResource::OtherLightData CreatePointLightData(const Light& light, Math::Vector4 shadowData)
        {
            ShaderResource::OtherLightData data;
            data.color = light.GetColor();
            data.direction = Math::Vector4::Zero();
            data.positionAndRange = Math::Vector4{light.GetPosition(), 1 / light.GetRange()};
            float angle = std::numbers::pi;
            data.spotAngle = Math::Vector4{angle * 0.5f, angle};   // 计算出的聚光灯衰减为 1
            data.shadowData = std::move(shadowData);
            return data;
        }

        /// @brief 
        /// @param light
        /// @param shadowData x 分量储存了阴影强度，若小于等于 0 则不进行阴影计算，
        ///                 y 分量存储了该光源在阴影图中的索引
        ///                 z 分量存储了该光源是否为点光源，若为 1 则是点光源，否则为聚光灯
        /// @return
        ShaderResource::OtherLightData CreateSpotLightData(const Light& light, Math::Vector4 shadowData)
        {
            ShaderResource::OtherLightData data;
            data.color = light.GetColor();
            data.direction = -Math::Vector4(light.GetDirection()).Normalized();
            data.positionAndRange = Math::Vector4{light.GetPosition(), 1 / light.GetRange()};
            data.spotAngle = Math::Vector4{light.GetInnerAngle(), light.GetOuterAngle()};
            data.shadowData = std::move(shadowData);
            return data;
        }

        void CreateBindingSet(IDevice* device)
        {
            auto depthTex = RenderResource::GetInstance().GetCommonTexture(CommonTextureSlot::Depth);
            m_BindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::StructuredBuffer_UAV(0, sm_TileInfoBuffer))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(0, m_LightBoundsBuffer))
                .AddItem(BindingSetItem::Texture_SRV(1, depthTex))
                .AddItem(BindingSetItem::ConstantBuffer(0, m_TileBasedLightCB))
                , m_BindingLayout);
        }

    public:
        static constexpr size_t sm_MaxDirLightCount = 4;
        static constexpr size_t sm_MaxOtherLightCount = 128;

        inline static LightSettings sm_Settings;

        inline static BufferHandle sm_LightDataBuffer{};
        inline static BufferHandle sm_DirLightDataBuffer{};
        inline static BufferHandle sm_OtherLightDataBuffer{};
        inline static BufferHandle sm_TileInfoBuffer{};

    private:
        std::unique_ptr<Shadows> m_Shadows;

        std::array<ComputePipelineHandle, 3> m_TileBasedLightPipelines;

        BufferHandle m_TileBasedLightCB{};
        BufferHandle m_LightBoundsBuffer{};

        BindingLayoutHandle m_BindingLayout;
        BindingSetHandle m_BindingSet{};
    };
}

#endif

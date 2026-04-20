#pragma once
#ifndef __LIGHTINGPASS_H__
#define __LIGHTINGPASS_H__

#include "RenderResource.h"
#include "Shadows.h"
#include "Runtime/Math/MathCommon.h"
#include "Shaders/ForwardShader/ResourceData.h"

#include <algorithm>

namespace DSM {
    class LightingPass : public IRenderPass
    {
    public:
        LightingPass(GraphicsRenderer& renderer)
            : m_Shadows(std::make_unique<Shadows>(renderer, ShadowSetting{}))
        {
            auto device = renderer.GetDevice();
            sm_LightDataBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(Math::Align(sizeof(ShaderResource::LightData), size_t(c_ConstantBufferOffsetSizeAlignment)))
                .SetIsConstantBuffer(true)
                .SetDebugName("Light Data Buffer"));
            sm_DirLightDataBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(sm_MaxDirLightCount * sizeof(ShaderResource::DirectionalLightData))
                .SetStructStride(sizeof(ShaderResource::DirectionalLightData))
                .SetDebugName("Directional Light Data Buffer"));
            sm_OtherLightDataBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(sm_MaxOtherLightCount * sizeof(ShaderResource::OtherLightData))
                .SetStructStride(sizeof(ShaderResource::OtherLightData))
                .SetDebugName("Other Light Data Buffer"));

            sm_TimerQuery = renderer.GetDevice()->CreateTimerQuery();
        }

        virtual ~LightingPass()
        {
            sm_LightDataBuffer = nullptr;
            sm_DirLightDataBuffer = nullptr;
            sm_OtherLightDataBuffer = nullptr;
        }

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            auto lights = DSMEngine::sm_GlobalContext.scene->GetObjectsWithComponents<Light>();
            if(lights.empty())
                return 0;

            m_Shadows->Setup();

            std::vector<ShaderResource::DirectionalLightData> dirLightData{};
            std::vector<ShaderResource::OtherLightData> otherLightData{};

            std::vector<std::pair<float, const Light*>> sortedLights{};

            auto camPos = renderer.GetCamera().GetPosition();
            auto camForward = renderer.GetCamera().GetLookAxis().Normalized();

            for (const auto& [id, light] : lights.each()) {
                if(auto obj = light.GetGameObject(); obj == nullptr || !obj->IsEnabled())
                    continue;

                float signedDistance = Math::Vector3::Dot(light.GetPosition() - camPos, camForward);
                signedDistance += signedDistance >= 0 ? signedDistance : (std::numeric_limits<float>::max() - signedDistance);
                sortedLights.emplace_back(signedDistance, &light);
            }

            std::sort(sortedLights.begin(), sortedLights.end(),
                [](const auto& lhs, const auto& rhs) {
                    return lhs.second < rhs.second;
                });

            for (const auto& [signedDistance, light] : sortedLights) {
                switch(light->GetType()){
                case LightType::Directional:
                    dirLightData.push_back(CreateDirLightData(*light, m_Shadows->ReserveDirectionalShadows(*light)));
                    break;
                case LightType::Point:
                    otherLightData.push_back(CreatePointLightData(*light, m_Shadows->ReserveOtherShadows(*light)));
                    break;
                case LightType::Spot:
                    otherLightData.push_back(CreateSpotLightData(*light, m_Shadows->ReserveOtherShadows(*light)));
                    break;
                default:
                    DSM_ERROR("Error light type.");
                    return 0;
                }
            }

            auto cmdList = renderer.GetDevice()->CreateCommandList(CommandListParameters().SetDebugName("Lighting Pass Command List"));
            cmdList->Open();

            cmdList->BeginTimerQuery(sm_TimerQuery);

            ShaderResource::LightData lightData;
            lightData.dirLightCount = std::min(dirLightData.size(), sm_MaxDirLightCount);
            lightData.otherLightCount = std::min(otherLightData.size(), sm_MaxOtherLightCount);
            cmdList->WriteBuffer(sm_LightDataBuffer, &lightData, sizeof(lightData));
            cmdList->WriteBuffer(sm_DirLightDataBuffer, dirLightData.data(), lightData.dirLightCount * sizeof(ShaderResource::DirectionalLightData));
            cmdList->WriteBuffer(sm_OtherLightDataBuffer, otherLightData.data(), lightData.otherLightCount * sizeof(ShaderResource::OtherLightData));

            cmdList->EndTimerQuery(sm_TimerQuery);

            cmdList->Close();
            auto fenceValue = renderer.GetDevice()->ExecuteCommandList(cmdList);

            auto shadowFenceValue = m_Shadows->Render(renderer, deltaTime);
            
            return std::max(fenceValue, shadowFenceValue);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            m_Shadows->OnResize(renderer, width, height);
        }

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

    public:
        static constexpr size_t sm_MaxDirLightCount = 4;
        static constexpr size_t sm_MaxOtherLightCount = 120;

        inline static TimerQueryHandle sm_TimerQuery{};

        inline static BufferHandle sm_LightDataBuffer{};
        inline static BufferHandle sm_DirLightDataBuffer{};
        inline static BufferHandle sm_OtherLightDataBuffer{};

    private:
        std::unique_ptr<Shadows> m_Shadows;
    };
}

#endif
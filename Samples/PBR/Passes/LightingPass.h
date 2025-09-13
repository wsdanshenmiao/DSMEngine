#pragma once
#ifndef __LIGHTINGPASS_H__
#define __LIGHTINGPASS_H__

#include "IRenderPass.h"
#include "Runtime/Math/MathCommon.h"
#include "Shaders/ResourceData.h"

namespace DSM {

    class LightingPass : public IRenderPass
    {
    private:
        struct LightData
        {
            int dirLightCount;
        };
    public:
        LightingPass(Renderer& renderer)
        {
            m_DirLightDataBuffer = renderer.GetDevice()->CreateBuffer(BufferDesc()
                .SetByteSize(sm_MaxDirLightCount * sizeof(DirectionalLightData))
                .SetStructStride(sizeof(DirectionalLightData))
                .SetDebugName("Directional Light Data Buffer"));
            m_LightDataBuffer = renderer.GetDevice()->CreateBuffer(BufferDesc()
                .SetByteSize(Math::Align(sizeof(LightData), size_t(c_ConstantBufferOffsetSizeAlignment)))
                .SetIsConstantBuffer(true)
                .SetDebugName("Light Data Buffer"));

            g_RenderResources.bindingLayoutDesc
                .AddItem(BindingLayoutItem().ConstantBuffer(3)) // Light Data
                .AddItem(BindingLayoutItem().StructuredBuffer_SRV(6)); // Directional Light Data
            g_RenderResources.bindingSetDesc
                .AddItem(BindingSetItem().ConstantBuffer(3, m_LightDataBuffer))
                .AddItem(BindingSetItem().StructuredBuffer_SRV(6, m_DirLightDataBuffer));
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            auto lightSize = g_RenderResources.lights.size();
            if(lightSize == 0)
                return;

            std::vector<DirectionalLightData> dirLightData{};

            for (const auto& light : g_RenderResources.lights) {
                switch(light.lightType){
                case LightType::Directional:
                    dirLightData.push_back(CreateDirLightData(light));
                    break;
                case LightType::Point:
                    break;
                case LightType::Spot:
                    break;
                default:
                    DSM_ERROR("Error light type.");
                    return;
                }
            }

            auto cmdList = renderer.GetDevice()->CreateCommandList(
                CommandListParameters().SetDebugName("LightingPassCmdList"));
            cmdList->Open();

            LightData lightData;
            lightData.dirLightCount = static_cast<int>(std::min(dirLightData.size(), sm_MaxDirLightCount));
            cmdList->WriteBuffer(m_LightDataBuffer, &lightData, sizeof(lightData));
            cmdList->WriteBuffer(m_DirLightDataBuffer, dirLightData.data(), lightData.dirLightCount * sizeof(DirectionalLightData));

            cmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(cmdList);
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) {}

    private:
        DirectionalLightData CreateDirLightData(const Light& light)
        {
            DirectionalLightData data;
            data.color = light.color;
            data.direction = -Math::Vector4(light.transform.GetForwardAxis());
            return data;
        }

    private:
        static constexpr size_t sm_MaxDirLightCount = 4;

        BufferHandle m_DirLightDataBuffer;
        BufferHandle m_LightDataBuffer;
    };
}

#endif
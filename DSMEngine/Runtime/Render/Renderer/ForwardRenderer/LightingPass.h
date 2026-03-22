#pragma once
#ifndef __LIGHTINGPASS_H__
#define __LIGHTINGPASS_H__

#include "IRenderPass.h"
#include "Runtime/Math/MathCommon.h"
#include "Shaders/ForwardShader/ResourceData.h"

namespace DSM {
    class LightingPass : public IRenderPass
    {
    public:
        LightingPass(Renderer& renderer)
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

            CreateShader(renderer);
            sm_TimerQuery = renderer.GetDevice()->CreateTimerQuery();
        }

        virtual ~LightingPass()
        {
            sm_LightDataBuffer = nullptr;
            sm_DirLightDataBuffer = nullptr;
            sm_OtherLightDataBuffer = nullptr;
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            auto lightSize = g_RenderResources.lights.size();
            if(lightSize == 0)
                return;

            std::vector<ShaderResource::DirectionalLightData> dirLightData{};
            std::vector<ShaderResource::OtherLightData> otherLightData{};

            for (const auto& light : g_RenderResources.lights) {
                switch(light.lightType){
                case LightType::Directional:
                    dirLightData.push_back(CreateDirLightData(light));
                    break;
                case LightType::Point:
                    otherLightData.push_back(CreatePointLightData(light));
                    break;
                case LightType::Spot:
                    otherLightData.push_back(CreateSpotLightData(light));
                    break;
                default:
                    DSM_ERROR("Error light type.");
                    return;
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
            renderer.GetDevice()->ExecuteCommandList(cmdList);
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) {}

    private:
        ShaderResource::DirectionalLightData CreateDirLightData(const Light& light)
        {
            ShaderResource::DirectionalLightData data;
            data.color = light.color;
            data.direction = -Math::Vector4(light.direction).Normalized();
            return data;
        }

        ShaderResource::OtherLightData CreatePointLightData(const Light& light)
        {
            ShaderResource::OtherLightData data;
            data.color = light.color;
            data.direction = Math::Vector4::Zero();
            data.positionAndRange = Math::Vector4{light.position, 1 / light.range};
            float angle = std::numbers::pi;
            data.spotAngle = Math::Vector4{angle * 0.5f, angle};   // 计算出的聚光灯衰减为 1
            return data;
        }

        ShaderResource::OtherLightData CreateSpotLightData(const Light& light)
        {
            ShaderResource::OtherLightData data;
            data.color = light.color;
            data.direction = -Math::Vector4(light.direction).Normalized();
            data.positionAndRange = Math::Vector4{light.position, 1 / light.range};
            data.spotAngle = Math::Vector4{light.innerAngle, light.outerAngle};
            return data;
        }

        void CreateShader(Renderer& renderer) 
        {
            // 创建着色器
            ShaderCompileDesc litVSDesc{};
            litVSDesc.SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/ForwardShader/Passes/LitPass.hlsl")
                .SetEnterPoint("LitPassVS");
            ShaderByteCode litVSNoTangent{litVSDesc};
            ShaderByteCode litVS{litVSDesc.AddDefine("USE_TANGENT", "1")};

            ShaderCompileDesc litPSDesc{};
            litPSDesc.SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/ForwardShader/Passes/LitPass.hlsl")
                .SetEnterPoint("LitPassPS");
            ShaderByteCode litPSNoTangent{litPSDesc};
            ShaderByteCode litPSNoTangentPCF3{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF3", "1")};
            ShaderByteCode litPSNoTangentPCF5{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF5", "1")};
            ShaderByteCode litPSNoTangentPCF7{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF7", "1")};
            ShaderByteCode litPS{litPSDesc.AddDefine("USE_TANGENT", "1")};
            ShaderByteCode litPSPCF3{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF3", "1")};
            ShaderByteCode litPSPCF5{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF5", "1")};
            ShaderByteCode litPSPCF7{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF7", "1")};

            auto createShader = [&](const ShaderByteCode& byteCode, const auto& name) {
                return renderer.GetDevice()->CreateShader(ShaderDesc()
                    .SetEntryName(byteCode.GetDesc().enterPoint)
                    .SetShaderType(byteCode.GetDesc().type)
                    .SetDebugName(name), 
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };
            auto& shaders = g_RenderResources.shaders;
            shaders[(size_t)ShaderSlot::LitVS] = createShader(litVS, "LitPassVS");
            shaders[(size_t)ShaderSlot::LitVSNoTangent] = createShader(litVSNoTangent, "LitPassVSNoTangent");
            shaders[(size_t)ShaderSlot::LitPS] = createShader(litPS, "LitPassPS");
            shaders[(size_t)ShaderSlot::LitPSPCF3] = createShader(litPSPCF3, "LitPassPSPCF3");
            shaders[(size_t)ShaderSlot::LitPSPCF5] = createShader(litPSPCF5, "LitPassPSPCF5");
            shaders[(size_t)ShaderSlot::LitPSPCF7] = createShader(litPSPCF7, "LitPassPSPCF7");
            shaders[(size_t)ShaderSlot::LitPSNoTangent] = createShader(litPSNoTangent, "LitPassPSNoTangent");
            shaders[(size_t)ShaderSlot::LitPSNoTangentPCF3] = createShader(litPSNoTangentPCF3, "LitPassPSNoTangentPCF3");
            shaders[(size_t)ShaderSlot::LitPSNoTangentPCF5] = createShader(litPSNoTangentPCF5, "LitPassPSNoTangentPCF5");
            shaders[(size_t)ShaderSlot::LitPSNoTangentPCF7] = createShader(litPSNoTangentPCF7, "LitPassPSNoTangentPCF7");
        }

    public:
        static constexpr size_t sm_MaxDirLightCount = 4;
        static constexpr size_t sm_MaxOtherLightCount = 120;

        inline static TimerQueryHandle sm_TimerQuery{};

        inline static BufferHandle sm_LightDataBuffer{};
        inline static BufferHandle sm_DirLightDataBuffer{};
        inline static BufferHandle sm_OtherLightDataBuffer{};    
    };
}

#endif
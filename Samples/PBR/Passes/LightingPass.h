#pragma once
#ifndef __LIGHTINGPASS_H__
#define __LIGHTINGPASS_H__

#include "IRenderPass.h"
#include "Runtime/Math/MathCommon.h"
#include "Shaders/ResourceData.h"

namespace DSM {
    class LightingPass : public IRenderPass
    {
    public:
        LightingPass(Renderer& renderer)
        {
            auto device = renderer.GetDevice();
            m_DirLightDataBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(sm_MaxDirLightCount * sizeof(DirectionalLightData))
                .SetStructStride(sizeof(DirectionalLightData))
                .SetDebugName("Directional Light Data Buffer"));
            m_LightDataBuffer = device->CreateBuffer(BufferDesc()
                .SetByteSize(Math::Align(sizeof(LightData), size_t(c_ConstantBufferOffsetSizeAlignment)))
                .SetIsConstantBuffer(true)
                .SetDebugName("Light Data Buffer"));

            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Common]
                .AddItem(BindingLayoutItem().ConstantBuffer(3)) // Light Data
                .AddItem(BindingLayoutItem().StructuredBuffer_SRV(6)); // Directional Light Data
            g_RenderResources.commonBindingSetDesc
                .AddItem(BindingSetItem().ConstantBuffer(3, m_LightDataBuffer))
                .AddItem(BindingSetItem().StructuredBuffer_SRV(6, m_DirLightDataBuffer));

            CreateShader(renderer);
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
            data.direction = -Math::Vector4(light.direction);
            return data;
        }

        OtherLightData CreatePointLightData(const Light& light)
        {
            OtherLightData data;
            data.color = light.color;
            data.direction = -Math::Vector4(light.direction);
            data.positionAndRange = Math::Vector4{light.position, light.range};
            data.spotAngle = Math::Vector4{light.innerAngle, light.outerAngle};
            return data;
        }


        void CreateShader(Renderer& renderer) 
        {

            // 创建着色器
            ShaderCompileDesc litVSDesc{};
            litVSDesc.SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/LitPass.hlsl")
                .SetEnterPoint("LitPassVS");
            ShaderByteCode litVSNoTangent{litVSDesc};
            ShaderByteCode litVS{litVSDesc.AddDefine("USE_TANGENT", "1")};

            ShaderCompileDesc litPSDesc{};
            litPSDesc.SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/LitPass.hlsl")
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

    private:
        static constexpr size_t sm_MaxDirLightCount = 4;

        BufferHandle m_DirLightDataBuffer;
        BufferHandle m_LightDataBuffer;
    };
}

#endif
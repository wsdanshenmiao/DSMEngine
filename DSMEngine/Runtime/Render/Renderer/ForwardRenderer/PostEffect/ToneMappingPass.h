#pragma once
#ifndef __TONEMAPPINGPASS_H__
#define __TONEMAPPINGPASS_H__

#include "PostEffectManager.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Math/MathCommon.h"
#include "Shaders/ForwardShader/ResourceData.h"

#include <array>
#include <string>

namespace DSM {
	enum class ToneMappingType
	{
		Reinhard = 0,
        ReinhardExtended,
		ACESFitted,
        ACEAApprox,
		Uncharted2,
		ToneMappingCount
	};

	struct ToneMappingSettings
	{
		ToneMappingType method = ToneMappingType::ACESFitted;
		float exposure = 1.0f;
        float whitePoint = 11.2f; // 仅在使用 ReinhardExtended 时有效
        bool gammaCorrection = true;
	};

	class ToneMappingPass : public IPostEffect
	{
	public:
		ToneMappingPass(Renderer& renderer)
		{
			m_Priority = std::numeric_limits<size_t>::max();

			auto device = renderer.GetDevice();
			auto bindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
				.SetVisibility(ShaderType::Compute)
				.AddItem(BindingLayoutItem::PushConstants(0, sizeof(ToneMappingConstants)))
				.AddItem(BindingLayoutItem::Texture_UAV(0))
				.AddItem(BindingLayoutItem::Texture_SRV(0)));

			for (uint32_t i = 0; i < (size_t)ToneMappingType::ToneMappingCount * 2; ++i) {
				auto byteCodeDesc = ShaderCompileDesc()
					.SetType(ShaderType::Compute)
					.SetMode(ShaderMode::SM_6_6)
					.SetFilename("Shaders/ForwardShader/PostEffect/ToneMapping.hlsl")
					.SetEnterPoint("ToneMappingCS")
					.AddDefine("TONEMAP_TYPE", std::to_string(i));
                if(i < (size_t)ToneMappingType::ToneMappingCount){
                    byteCodeDesc.AddDefine("TONEMAP_APPLY_GAMMA", "1");
                }
                auto csByteCode = ShaderByteCode{byteCodeDesc};
				auto cs = device->CreateShader(ShaderDesc()
					.SetShaderType(ShaderType::Compute)
					.SetDebugName("Tone Mapping Compute Shader " + std::to_string(i))
					.SetEntryName("ToneMappingCS"),
					csByteCode.GetByteCode(), csByteCode.GetByteCodeSize());

				m_Pipelines[i] = device->CreateComputePipeline(ComputePipelineDesc{}
					.AddBindingLayout(bindingLayout, 0)
					.SetComputeShader(cs));
			}
		}

		void SetSettings(const ToneMappingSettings& settings) { m_Settings = settings; }

		void Render(Renderer& renderer, ICommandList* cmdList, float deltaTime, ITexture* srcTex, ITexture* dstTex) override
		{
			auto device = renderer.GetDevice();
			if (srcTex != m_CacheSrcTex || dstTex != m_CacheDstTex) {
				m_CacheSrcTex = srcTex;
				m_CacheDstTex = dstTex;
				m_BindingSet = device->CreateBindingSet(BindingSetDesc()
					.AddItem(BindingSetItem::PushConstants(0, sizeof(ToneMappingConstants)))
					.AddItem(BindingSetItem::Texture_UAV(0, dstTex))
					.AddItem(BindingSetItem::Texture_SRV(0, srcTex)),
					m_Pipelines[0]->GetDesc().bindingLayouts[0]);
			}

            cmdList->Open();

            cmdList->SetTextureState(srcTex, AllSubresources, ResourceStates::NoPixelShaderResource);
            cmdList->SetTextureState(dstTex, AllSubresources, ResourceStates::UnorderedAccess);

			int methodIndex = std::min(int(m_Settings.method), (int)ToneMappingType::ToneMappingCount - 1);
			if(!m_Settings.gammaCorrection){
                methodIndex += (size_t)ToneMappingType::ToneMappingCount;
            }
            cmdList->SetComputeState(ComputeState()
				.SetPipeline(m_Pipelines[methodIndex])
				.AddBindingSet(m_BindingSet));

			ToneMappingConstants toneMappingConstants = { m_Settings.exposure, m_Settings.whitePoint };
			cmdList->SetPushConstants(&toneMappingConstants, sizeof(ToneMappingConstants));

			uint32_t groupX = Math::DivideByMultiple(srcTex->GetDesc().width, sm_ThreadSize);
			uint32_t groupY = Math::DivideByMultiple(srcTex->GetDesc().height, sm_ThreadSize);
			cmdList->Dispatch(groupX, groupY);
            
            cmdList->Close();
            device->ExecuteCommandList(cmdList);
		}

    private:
        struct ToneMappingConstants
        {
            float exposure;
            float whitePoint; // 仅在使用 ReinhardExtended 时有效
        };

		static constexpr uint32_t sm_ThreadSize = 8;

        ToneMappingSettings m_Settings{};		
        std::array<ComputePipelineHandle, (size_t)ToneMappingType::ToneMappingCount * 2> m_Pipelines{};

		ITexture* m_CacheSrcTex = nullptr;
		ITexture* m_CacheDstTex = nullptr;
		BindingSetHandle m_BindingSet{};
	};
}


#endif // __TONEMAPPINGPASS_H__

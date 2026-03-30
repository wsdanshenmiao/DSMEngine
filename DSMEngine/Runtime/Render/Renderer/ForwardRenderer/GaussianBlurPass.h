#pragma once
#ifndef __GAUSSIANBLURPASS_H__
#define __GAUSSIANBLURPASS_H__

#include "RenderResource.h"
#include "Runtime/Math/MathCommon.h"
#include "Runtime/Render/ShaderCompiler.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace DSM {
	struct GaussianBlurSettings
	{
		uint32_t blurRadius = 5;
		uint32_t blurCount = 1;
	};

	class GaussianBlurPass : public IRenderPass
	{
	public:
		explicit GaussianBlurPass(Renderer& renderer)
		{
			auto device = renderer.GetDevice();

			m_BlurConstants = device->CreateBuffer(BufferDesc()
				.SetByteSize(sizeof(BlurConstants))
				.SetIsConstantBuffer(true)
				.SetIsVolatile(true)
				.SetDebugName("Gaussian Blur Constants"));
			m_BlurWeights = device->CreateBuffer(BufferDesc()
				.SetByteSize(sizeof(float) * (sm_MaxBlurRadius * 2 + 1))
				.SetStructStride(sizeof(float))
				.SetDebugName("Gaussian Blur Weights"));

			m_BindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
				.SetVisibility(ShaderType::Compute)
				.AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
				.AddItem(BindingLayoutItem::Texture_UAV(0))
				.AddItem(BindingLayoutItem::Texture_SRV(0))
				.AddItem(BindingLayoutItem::StructuredBuffer_SRV(1)));

			ShaderByteCode blurCsByteCode{ShaderCompileDesc()
				.SetType(ShaderType::Compute)
				.SetMode(ShaderMode::SM_6_6)
				.SetEnterPoint("GaussianBlurCS")
				.SetFilename("Shaders/ForwardShader/Passes/GaussianBlur.hlsl")};
			auto blurCs = device->CreateShader(ShaderDesc()
				.SetShaderType(ShaderType::Compute)
				.SetEntryName("GaussianBlurCS")
				.SetDebugName("Gaussian Blur Compute Shader"),
				blurCsByteCode.GetByteCode(), blurCsByteCode.GetByteCodeSize());
			m_Pipeline = device->CreateComputePipeline(ComputePipelineDesc()
				.SetComputeShader(blurCs)
				.AddBindingLayout(m_BindingLayout, 0));
		}

		void SetTargetTexture(TextureHandle texture, GaussianBlurSettings settings)
		{
            m_Settings = std::move(settings);
			if (m_TargetTexture == texture) {
				return;
			}
			m_TargetTexture = texture;
			InvalidateCache();
		}

		void BlurTexture(Renderer& renderer, ICommandList* cmdList, ITexture* targetTexture, const GaussianBlurSettings& settings)
		{
			if (cmdList == nullptr || targetTexture == nullptr || settings.blurCount < 1) {
				return;
			}

			uint32_t blurRadius = std::clamp(settings.blurRadius, 0u, sm_MaxBlurRadius);

            // 创建纹理及绑定资源
			EnsureResources(renderer, targetTexture);
			if (m_TmpTexture == nullptr || m_BlurToTargetBindingSet == nullptr || m_BlurToTmpBindingSet == nullptr) {
				return;
			}

            // 更新高斯权重
			if (m_CachedBlurRadius != blurRadius) {
				std::vector<float> weights = CalculateGaussWeights(blurRadius);
				cmdList->WriteBuffer(m_BlurWeights, weights.data(), sizeof(float) * weights.size());
				m_CachedBlurRadius = blurRadius;
			}

            // 若目标纹理不支持 UAV，则使用额外的 UAV 纹理作为中转，模糊完成后再将结果复制回目标纹理
            auto targetTex = targetTexture;
            if(m_UseUAV) {
                targetTex = m_UAVTexture;
                cmdList->CopyTexture(m_UAVTexture, {}, targetTexture, {});
            }

			BlurConstants blurConstants{};
			blurConstants.blurRadius = blurRadius;
			for (uint32_t i = 0; i < settings.blurCount; ++i) {
                // 水平模糊
				blurConstants.isHorizontal = 1;
				cmdList->WriteBuffer(m_BlurConstants, &blurConstants, sizeof(blurConstants));

				cmdList->SetTextureState(targetTex, AllSubresources, ResourceStates::NoPixelShaderResource);
				cmdList->SetTextureState(m_TmpTexture, AllSubresources, ResourceStates::UnorderedAccess);
				cmdList->SetComputeState(ComputeState()
					.SetPipeline(m_Pipeline)
					.AddBindingSet(m_BlurToTmpBindingSet));

				uint32_t groupX = Math::Align(m_TmpTexture->GetDesc().width, sm_ThreadSize) / sm_ThreadSize;
				cmdList->Dispatch(groupX, m_TmpTexture->GetDesc().height, 1);


                // 垂直模糊
				blurConstants.isHorizontal = 0;
				cmdList->WriteBuffer(m_BlurConstants, &blurConstants, sizeof(blurConstants));

				cmdList->SetTextureState(m_TmpTexture, AllSubresources, ResourceStates::NoPixelShaderResource);
				cmdList->SetTextureState(targetTex, AllSubresources, ResourceStates::UnorderedAccess);
				cmdList->SetComputeState(ComputeState()
					.SetPipeline(m_Pipeline)
					.AddBindingSet(m_BlurToTargetBindingSet));

				groupX = Math::Align(m_TmpTexture->GetDesc().height, sm_ThreadSize) / sm_ThreadSize;
				cmdList->Dispatch(groupX, targetTex->GetDesc().width, 1);
			}

            if(m_UseUAV) {
                // 将模糊结果从 UAV 纹理复制回目标纹理
                cmdList->CopyTexture(targetTexture, {}, m_UAVTexture, {});
            }
		}

		uint64_t Render(Renderer& renderer, float deltaTime) override
		{
			if (m_TargetTexture == nullptr) {
				return 0;
			}

			auto cmdList = renderer.GetDevice()->CreateCommandList(CommandListParameters()
				.SetDebugName("GaussianBlur Command List")
				.SetQueueType(CommandQueueType::Compute));
			cmdList->Open();
			BlurTexture(renderer, cmdList, m_TargetTexture, m_Settings);
			cmdList->Close();
			return renderer.GetDevice()->ExecuteCommandList(cmdList);
		}

		void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override {}

	private:
		struct BlurConstants
		{
			uint32_t blurRadius = 0;
			uint32_t isHorizontal = 0;
			uint32_t pad0 = 0;
			uint32_t pad1 = 0;
		};

		void InvalidateCache()
		{
            m_CachedTargetTexture = nullptr;
            m_UAVTexture = nullptr;
			m_TmpTexture = nullptr;
			m_BlurToTargetBindingSet = nullptr;
			m_BlurToTmpBindingSet = nullptr;
		}

		void EnsureResources(Renderer& renderer, ITexture* targetTexture)
		{
			if (targetTexture == nullptr) {
				return;
			}

            if(m_CachedTargetTexture == targetTexture && 
                m_BlurToTmpBindingSet != nullptr && 
                m_BlurToTargetBindingSet != nullptr &&
                m_TmpTexture != nullptr && 
                (!m_UseUAV || m_UAVTexture != nullptr)) {
                return;
            }

			const auto& targetDesc = targetTexture->GetDesc();
			m_UseUAV = !targetDesc.isUAV;

			auto device = renderer.GetDevice();
			auto tmpDesc = TextureDesc{targetDesc}
                .SetIsUAV(true)
			    .SetIsRenderTarget(false)
			    .SetKeepInitialState(true)
			    .SetDebugName("Gaussian Blur Tmp")
			    .SetInitialState(ResourceStates::UnorderedAccess);
			m_TmpTexture = device->CreateTexture(tmpDesc);
            if(m_UseUAV){
                m_UAVTexture = device->CreateTexture(TextureDesc{tmpDesc}.SetDebugName("Gaussian Blur UAV"));
            }

			m_BlurToTargetBindingSet = device->CreateBindingSet(BindingSetDesc()
				.AddItem(BindingSetItem::ConstantBuffer(0, m_BlurConstants))
				.AddItem(BindingSetItem::Texture_UAV(0, m_UseUAV ? m_UAVTexture.Get() : targetTexture))
				.AddItem(BindingSetItem::Texture_SRV(0, m_TmpTexture))
				.AddItem(BindingSetItem::StructuredBuffer_SRV(1, m_BlurWeights)),
				m_BindingLayout);
			m_BlurToTmpBindingSet = device->CreateBindingSet(BindingSetDesc()
				.AddItem(BindingSetItem::ConstantBuffer(0, m_BlurConstants))
				.AddItem(BindingSetItem::Texture_UAV(0, m_TmpTexture))
				.AddItem(BindingSetItem::Texture_SRV(0, m_UseUAV ? m_UAVTexture.Get() : targetTexture))
				.AddItem(BindingSetItem::StructuredBuffer_SRV(1, m_BlurWeights)),
				m_BindingLayout);

            m_CachedTargetTexture = targetTexture;
		}

		float GaussFunction(float x, float sigma) const
		{
			return std::exp(-x * x / (2.0f * sigma * sigma));
		}

		std::vector<float> CalculateGaussWeights(uint32_t blurRadius) const
		{
            // 取 2 sigma 范围，计算高斯核范围
            float sigma = (float)blurRadius / 2.0f;
			std::vector<float> weights(2 * blurRadius + 1);
			for (int i = -blurRadius; i <= int(blurRadius); ++i) {
				weights[i + blurRadius] = GaussFunction(float(i), sigma);
			}
			return weights;
		}

	public:
		inline static constexpr uint32_t sm_MaxBlurRadius = 10;
		inline static constexpr uint32_t sm_ThreadSize = 256;

	private:
        GaussianBlurSettings m_Settings{};
        bool m_UseUAV = false;

		uint32_t m_CachedBlurRadius = std::numeric_limits<uint32_t>::max();
        ITexture* m_CachedTargetTexture = nullptr;

		BufferHandle m_BlurConstants;
		BufferHandle m_BlurWeights;

		TextureHandle m_TargetTexture{};
        TextureHandle m_UAVTexture{};
		TextureHandle m_TmpTexture{};

		BindingSetHandle m_BlurToTargetBindingSet;
		BindingSetHandle m_BlurToTmpBindingSet;

		BindingLayoutHandle m_BindingLayout;

		ComputePipelineHandle m_Pipeline;
	};

} // namespace DSM

#endif // !__GAUSSIANBLURPASS_H__

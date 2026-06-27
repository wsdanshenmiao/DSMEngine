#pragma once
#ifndef __MIPMAPPASS_H__
#define __MIPMAPPASS_H__

#include "RenderResource.h"
#include "Runtime/Math/MathCommon.h"
#include "Runtime/Render/ShaderCompiler.h"

namespace DSM {
	class MipmapPass : public IRenderPass
	{
	public:
		explicit MipmapPass(GraphicsRenderer& renderer)
		{
			auto device = renderer.GetDevice();

			m_MipmapConstants = device->CreateBuffer(BufferDesc()
				.SetByteSize(sizeof(MipmapConstants))
				.SetIsConstantBuffer(true)
				.SetIsVolatile(true)
				.SetDebugName("Mipmap Constants"));

			m_BindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
				.SetVisibility(ShaderType::Compute)
				.AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
				.AddItem(BindingLayoutItem::Texture_UAV(0))
				.AddItem(BindingLayoutItem::Texture_SRV(0))
				.AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::LinearClamp))));

			ShaderByteCode csByteCode{ShaderCompileDesc()
				.SetType(ShaderType::Compute)
				.SetMode(ShaderMode::SM_6_6)
				.SetEnterPoint("GenerateMipmapCS")
				.SetFilename("Shaders/ForwardShader/Passes/MipmapPass.hlsl")};
			auto cs = device->CreateShader(ShaderDesc()
				.SetShaderType(ShaderType::Compute)
				.SetEntryName("GenerateMipmapCS")
				.SetDebugName("Mipmap Compute Shader"),
				csByteCode.GetByteCode(), csByteCode.GetByteCodeSize());

			m_Pipeline = device->CreateComputePipeline(ComputePipelineDesc()
				.SetComputeShader(cs)
				.AddBindingLayout(m_BindingLayout, 0));
		}

		void SetTargetTexture(TextureHandle texture)
		{
			m_TargetTexture = texture;
			InvalidateBindingSetCache();
		}

		// 生成 texture 的完整 mip 链（从 baseMipLevel 开始）。
		void GenerateMips(GraphicsRenderer& renderer, ICommandList* cmdList, ITexture* texture, uint32_t baseMipLevel = 0)
		{
			if (cmdList == nullptr || texture == nullptr) {
				return;
			}

			const auto& texDesc = texture->GetDesc();
			if (!texDesc.isUAV || texDesc.mipLevels <= 1) {
				return;
			}

			if (baseMipLevel >= texDesc.mipLevels - 1) {
				return;
			}

			EnsureBindingSetCache(renderer, texture);
			for (uint32_t srcMip = baseMipLevel; srcMip + 1 < texDesc.mipLevels; ++srcMip) {
				const uint32_t dstMip = srcMip + 1;
				const uint32_t dstWidth = std::max(1u, texDesc.width >> dstMip);
				const uint32_t dstHeight = std::max(1u, texDesc.height >> dstMip);

				MipmapConstants constants{};
				constants.dstWidth = dstWidth;
				constants.dstHeight = dstHeight;
				cmdList->WriteBuffer(m_MipmapConstants, &constants, sizeof(constants));

				const TextureSubresourceSet srcSubresources(srcMip, 1, 0, 1);
				const TextureSubresourceSet dstSubresources(dstMip, 1, 0, 1);

				cmdList->SetTextureState(texture, srcSubresources, ResourceStates::NoPixelShaderResource);
				cmdList->SetTextureState(texture, dstSubresources, ResourceStates::UnorderedAccess);

				cmdList->SetComputeState(ComputeState()
					.SetPipeline(m_Pipeline)
					.AddBindingSet(m_BindingSetCache[srcMip]));
				cmdList->Dispatch(
					Math::DivideByMultiple(dstWidth, sm_ThreadGroupSize),
					Math::DivideByMultiple(dstHeight, sm_ThreadGroupSize),
					1);
			}
		}

		uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
		{
			if (m_TargetTexture == nullptr) {
				return 0;
			}

			auto cmdList = renderer.GetDevice()->CreateCommandList(CommandListParameters()
				.SetDebugName("MipmapPass Command List")
				.SetQueueType(CommandQueueType::Compute));
			cmdList->Open();

			GenerateMips(renderer, cmdList, m_TargetTexture);

			cmdList->Close();
			return renderer.GetDevice()->ExecuteCommandList(cmdList);
		}

		void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override { }

	private:
		void InvalidateBindingSetCache()
		{
			m_CachedTexture = nullptr;
			m_BindingSetCache.clear();
		}

		void EnsureBindingSetCache(GraphicsRenderer& renderer, ITexture* texture)
		{
			if (texture == nullptr) {
				return;
			}

			const auto& texDesc = texture->GetDesc();
			const uint32_t requiredBindingSetCount = texDesc.mipLevels > 0 ? texDesc.mipLevels - 1 : 0;
			if (m_CachedTexture == texture && m_BindingSetCache.size() == requiredBindingSetCount) {
				return;
			}

			m_CachedTexture = texture;
			m_BindingSetCache.clear();
			m_BindingSetCache.reserve(requiredBindingSetCount);

            auto sampler = RenderResource::GetInstance().GetCommonSampler(SamplerSlot::LinearClamp);
			for (uint32_t srcMip = 0; srcMip + 1 < texDesc.mipLevels; ++srcMip) {
				const uint32_t dstMip = srcMip + 1;
				const TextureSubresourceSet srcSubresources(srcMip, 1, 0, 1);
				const TextureSubresourceSet dstSubresources(dstMip, 1, 0, 1);

				auto bindingSet = renderer.GetDevice()->CreateBindingSet(BindingSetDesc()
					.AddItem(BindingSetItem::ConstantBuffer(0, m_MipmapConstants))
					.AddItem(BindingSetItem::Texture_UAV(0, texture, Format::UNKNOWN, dstSubresources))
					.AddItem(BindingSetItem::Texture_SRV(0, texture, Format::UNKNOWN, srcSubresources))
                    .AddItem(BindingSetItem::Sampler(size_t(SamplerSlot::LinearClamp), sampler)),
					m_BindingLayout);
				m_BindingSetCache.push_back(bindingSet);
			}
		}

	private:
		struct MipmapConstants
		{
			uint32_t dstWidth = 0;
			uint32_t dstHeight = 0;
		};

		inline static constexpr uint32_t sm_ThreadGroupSize = 8;

		TextureHandle m_TargetTexture{};
		ITexture* m_CachedTexture = nullptr;
		std::vector<BindingSetHandle> m_BindingSetCache{};

		BufferHandle m_MipmapConstants;
		BindingLayoutHandle m_BindingLayout;
		ComputePipelineHandle m_Pipeline;
	};
}

#endif // __MIPMAPPASS_H__

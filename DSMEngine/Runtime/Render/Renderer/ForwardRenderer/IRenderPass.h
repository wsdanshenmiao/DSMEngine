#pragma once
#ifndef __IRENDERPASS_H__
#define __IRENDERPASS_H__

#include "Runtime/Render/Light.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Math/Collision/BVH.h"

#include <array>

namespace DSM {

    struct IRenderPass
    {
        virtual ~IRenderPass() = default;
        virtual void Render(DSM::Renderer& renderer, float deltaTime) = 0;
        virtual void OnResize(Renderer& renderer, uint32_t width, uint32_t height) = 0;
    };

    struct RenderConfig
    {
        GraphicsPipelineDesc pipelineDesc;
        BufferHandle meshCB;
    };

    enum class ShaderSlot : uint32_t
    {
        LitVS,
        LitVSNoTangent,
        LitPS,
        LitPSPCF3,
        LitPSPCF5,
        LitPSPCF7,
        LitPSNoTangent,
        LitPSNoTangentPCF3,
        LitPSNoTangentPCF5,
        LitPSNoTangentPCF7,
        Count
    };

    enum class CommonTextureSlot : uint32_t
    {
        Color = 0,
        Depth,
        Normal, //  视图空间下的法线
        Noise,
        SSAO,
        ShadowMap,
        Count
    };

    enum class SamplerSlot : uint8_t
    {
        PointWrap = 0,
        LinearWrap,
        AnisoWrap,
        PointClamp,
        LinearClamp,
        AnisoClamp,
        PointBorder,
        LinearBorder,
        Shadow,
        Count
    };

    struct RenderResource
    {
        FramebufferHandle framebuffer;

        std::array<TextureHandle, (size_t)CommonTextureSlot::Count> commonTextures;
        std::array<SamplerHandle, (size_t)SamplerSlot::Count> commonSamplers;

        std::vector<ShaderHandle> shaders;
        std::vector<Light> lights;

        // 场景中的物体
        std::vector<std::shared_ptr<GameObject>> objects{};
        std::vector<std::pair<size_t, std::shared_ptr<GameObject>>> objInFrustum{};

        BufferHandle meshBuffer{};
        BufferHandle materialBuffer{};
        std::unordered_map<TextureHandle, size_t> textures{};
        BindingLayoutHandle textureBindlessLayout{};
        DescriptorTableHandle textureBindlessTable{};

        RenderResource();
    };

    extern RenderResource g_RenderResources;

    inline SamplerHandle GetCommonSampler(SamplerSlot slot)
    {
        return g_RenderResources.commonSamplers[static_cast<size_t>(slot)];
    }

    inline SamplerHandle GetCommonSampler(size_t slot)
    {
        assert(slot < (size_t)SamplerSlot::Count);
        return g_RenderResources.commonSamplers[slot];
    }

    inline TextureHandle GetCommonTexture(CommonTextureSlot slot)
    {
        return g_RenderResources.commonTextures[static_cast<size_t>(slot)];
    }

    inline TextureHandle GetCommonTexture(size_t slot)
    {
        assert(slot < (size_t)CommonTextureSlot::Count);
        return g_RenderResources.commonTextures[slot];
    }

} // namespace DSM

#endif
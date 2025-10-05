#pragma once
#ifndef __IRENDERPASS_H__
#define __IRENDERPASS_H__

#include "Runtime/Render/Renderer/Renderer.h"
#include "../Light.h"

namespace DSM {

    struct IRenderPass
    {
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
        ShadowVS,
        ShadowPS,
        ShadowVSClip,
        ShadowPSClip,
        Count
    };

    enum class BindingLayoutSlot : uint32_t
    {
        Common,
        Local,
        Count
    };

    enum class CommonTextureSlot : uint32_t
    {
        Color = 0,
        Depth,
        Normal, //  视图空间下的法线
        Noise,
        SSAO,
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

        std::unordered_map<GraphicsPipelineDesc, GraphicsPipelineHandle> psoCache;
        std::vector<RenderConfig> renderConfigs;
        
        std::vector<ShaderHandle> shaders;
        std::vector<Light> lights;

        // 每一个 Pass 根据其所提供的资源添加 Layout
        std::array<BindingLayoutDesc, (size_t)BindingLayoutSlot::Count> bindingLayoutDescs;
        std::array<BindingLayoutHandle, (size_t)BindingLayoutSlot::Count> bindingLayouts;

        // 每一个 Pass 根据其所提供的资源添加 Binding Set
        BindingSetDesc commonBindingSetDesc;
        BindingSetHandle commonBindingSet;

        CommandListHandle cmdList;

        RenderResource();
    };

    extern RenderResource g_RenderResources;

    inline SamplerHandle GetCommonSampler(SamplerSlot slot)
    {
        return g_RenderResources.commonSamplers[static_cast<size_t>(slot)];
    }

    inline BindingLayoutHandle GetBindingLayout(BindingLayoutSlot slot)
    {
        return g_RenderResources.bindingLayouts[static_cast<size_t>(slot)];
    }

    inline TextureHandle GetCommonTexture(CommonTextureSlot slot)
    {
        return g_RenderResources.commonTextures[static_cast<size_t>(slot)];
    }

} // namespace DSM

#endif
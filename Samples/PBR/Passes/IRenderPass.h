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

    struct RenderResource
    {
        FramebufferHandle framebuffer;

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

        RenderResource();
    };

    extern RenderResource g_RenderResources;

} // namespace DSM

#endif
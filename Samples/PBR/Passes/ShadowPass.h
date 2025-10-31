#pragma once
#ifndef __SHADOW_PASS_H__
#define __SHADOW_PASS_H__

#include <map>
#include "IRenderPass.h"

namespace DSM {
    namespace Math{
        class BoundingSphere;
    }


    struct ShadowSetting
    {
        constexpr static uint32_t sm_MaxCascadeCount = 4;

        enum MapSize
        {
            _256 = 256,
            _512 = 512,
            _1024 = 1024,
            _2048 = 2048
        };
        
        enum FilterMode
        {
            None,
            _PCF3x3,
            _PCF5x5,
            _PCF7x7
        };
        
        struct Directional
        {
            MapSize size = MapSize::_1024;
            FilterMode filter = FilterMode::_PCF3x3;
            uint32_t cascadeCount = sm_MaxCascadeCount;
            // 级联所占的百分比
            Math::Vector3 cascadeRatio = { 0.1f, 0.25f, 0.5f };
        };

        float distance = 100;
        float distanceFade = 0.1f;
        Directional directionalSetting{};
    };

    class ShadowPass : public IRenderPass
    {
    private:
        struct DrawShadowConstants
        {
            Math::Matrix4 viewProj;
            Math::Vector4 baseColor;
        };

    public:
        ShadowPass(Renderer& renderer, ShadowSetting shadowSetting);

        void Render(Renderer& renderer, float deltaTime) override;
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override{}


    private:
        void RenderDirectionalShadow(Renderer& renderer, const Math::BoundingSphere& boundingSphere, size_t index, size_t split, size_t tileSize);

        void DrawModelShadow(IDevice* device, DrawShadowConstants& shadowCB, Viewport viewport);

        Viewport GetTileViewport(size_t index, size_t split, size_t tileSize) const;

        Math::Matrix4 ConvertToAtlasMatrix(const Math::Matrix4& m, Math::Vector2 offset, float scale) const;


    public:
        inline static ShadowSetting sm_Setting;
        inline static TimerQueryHandle sm_TimerQuery{};

    private:
        static constexpr size_t sm_MaxShadowedDirectionalLightCount = 4;
        using ShadowMatrixArray = std::array<Math::Matrix4, sm_MaxShadowedDirectionalLightCount * ShadowSetting::sm_MaxCascadeCount>;


        TextureHandle m_ShadowMap;

        BufferHandle m_ShadowCB;
        ShadowMatrixArray m_DirectionalShadowMatrices{};

        FramebufferHandle m_ShadowFramebuffer;

        std::vector<GraphicsPipelineHandle> m_ShadowPipelineDescs;
        std::array<BindingLayoutHandle, 2> m_ShadowBindingLayouts;

        std::vector<Light> m_DirectionalLights{};
    };
} // namespace DSM


#endif
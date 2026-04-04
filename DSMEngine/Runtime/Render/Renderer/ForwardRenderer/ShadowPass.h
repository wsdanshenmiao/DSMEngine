#pragma once
#ifndef __SHADOW_PASS_H__
#define __SHADOW_PASS_H__

#include <map>
#include "RenderResource.h"
#include "Runtime/Math/Collision/Frustum.h"

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
            _PCF7x7,
            Count
        };
        
        struct Directional
        {
            MapSize size = MapSize::_1024;
            FilterMode filter = FilterMode::_PCF3x3;
            uint32_t cascadeCount = sm_MaxCascadeCount;
            // 级联所占的百分比
            Math::Vector3 cascadeRatio = { 0.1f, 0.25f, 0.5f };
            float cascadeFace = 0.1f;
        };

        float distance = 100.f;
        float distanceFade = 0.1f;
        Directional directionalSetting{};
    };

    class ShadowPass : public IRenderPass
    {
    public:
        ShadowPass(GraphicsRenderer& renderer, ShadowSetting shadowSetting);
        virtual ~ShadowPass();

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override;
        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override{}


    private:
        void RenderDirectionalShadow(GraphicsRenderer& renderer, const Math::BoundingSphere& boundingSphere, size_t index, size_t split, size_t tileSize);

        void DrawModelShadow(IDevice* device, const Math::Matrix4& viewProj, Viewport viewport, size_t cascadeIndex);

        Viewport GetTileViewport(size_t index, size_t split, size_t tileSize) const;

        Math::Matrix4 ConvertToAtlasMatrix(const Math::Matrix4& m, Math::Vector2 offset, float scale) const;

        void ResizeShadowMap(IDevice* device);

    public:
        inline static ShadowSetting sm_Setting;
        inline static BufferHandle sm_ShadowCB{};
        inline static TimerQueryHandle sm_TimerQuery{};

    private:
        enum ShaderSlot
        {
            ShadowVS,
            ShadowVSClip,
            ShadowPS,
            ShadowPSClip,
            Count
        };

        static constexpr size_t sm_MaxShadowedDirectionalLightCount = 4;
        using ShadowMatrixArray = std::array<Math::Matrix4, sm_MaxShadowedDirectionalLightCount * ShadowSetting::sm_MaxCascadeCount>;

        CommandListHandle m_CmdList;

        BufferHandle m_PassCB{};

        ShadowMatrixArray m_DirectionalShadowMatrices{};

        FramebufferHandle m_ShadowFramebuffer;

        std::vector<GraphicsPipelineHandle> m_ShadowPipeline;
        BindingLayoutHandle m_ShadowBindingLayout;

        std::vector<Light> m_DirectionalLights{};

        Math::Frustum m_CameraFrustum{};
    };
} // namespace DSM


#endif
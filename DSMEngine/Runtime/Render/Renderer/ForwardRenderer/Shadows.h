#pragma once
#ifndef __SHADOWS_H__
#define __SHADOWS_H__

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
            _2048 = 2048,
            _4096 = 4096,
            _8192 = 8192
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
            MapSize size = MapSize::_2048;
            FilterMode filter = FilterMode::_PCF3x3;
            uint32_t cascadeCount = sm_MaxCascadeCount;
            // 级联所占的百分比
            Math::Vector3 cascadeRatio = { 0.1f, 0.25f, 0.5f };
            float cascadeFace = 0.1f;
        };

        struct Other
        {
            MapSize size = MapSize::_1024;
            FilterMode filter = FilterMode::_PCF3x3;
        };

        float distance = 400.f;
        float distanceFade = 0.1f;
        Directional directionalSetting{};
        Other otherSetting{};
    };

    class Shadows
    {
    public:
        Shadows(GraphicsRenderer& renderer, ShadowSetting shadowSetting);
        ~Shadows();

        void Setup();

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime);
        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) {}

        Math::Vector4 ReserveDirectionalShadows(const Light& light);
        Math::Vector4 ReserveOtherShadows(const Light& light);

    private:
        void RenderDirectionalShadow(std::span<const Light*> directionalLights, const Camera& camera);
        void RenderDirectionalShadow(const Math::BoundingSphere& boundingSphere,
            std::span<const Light*> directionalLights,
            size_t index, size_t split, size_t tileSize);

        void RenderOtherShadow(std::span<const Light*> otherLights, const Camera& camera);
        void RenderPointLightShadow(const Light& light, size_t index);
        void RenderSpotLightShadow(const Light& light, size_t index);

        void DrawModelShadow(IFramebuffer* framebuffer, const Math::Matrix4& viewProj, Viewport viewport);

        Viewport GetTileViewport(size_t index, size_t split, size_t tileSize) const;

        Math::Matrix4 ConvertToAtlasMatrix(const Math::Matrix4& m, Math::Vector2 offset, float scale) const;

        void ResizeShadowMap(IDevice* device);

        size_t GetCubeMapFaceIndex(const Math::Vector3& direction) const;
        Math::Vector3 GetCubeMapFaceDirection(size_t faceIndex) const;

    public:
        inline static ShadowSetting sm_Setting;
        
        inline static BufferHandle sm_ShadowCB{};
        inline static BufferHandle sm_DirectionalShadowMatrixBuffer{};
        inline static BufferHandle sm_OtherShadowMatrixBuffer{};

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
        static constexpr size_t sm_MaxShadowedOtherLightCount = 16;
        using ShadowMatrixArray = std::array<Math::Matrix4, sm_MaxShadowedDirectionalLightCount * ShadowSetting::sm_MaxCascadeCount>;

        CommandListHandle m_CmdList;

        BufferHandle m_PassCB{};

        ShadowMatrixArray m_DirectionalShadowMatrices{};
        std::array<Math::Matrix4, sm_MaxShadowedOtherLightCount> m_OtherShadowMatrices{};
        std::vector<const Light*> m_ReservedDirectionalLights{};
        std::vector<const Light*> m_ReservedOtherLights{};
        size_t m_OtherLightShadowCount{0};

        FramebufferHandle m_DirectionalShadowFB;
        FramebufferHandle m_OtherShadowFB;

        std::vector<GraphicsPipelineHandle> m_ShadowPipeline;
        BindingLayoutHandle m_ShadowBindingLayout;

        BindingSetHandle m_ShadowBindingSet;

        Math::Frustum m_CameraFrustum{};
    };
} // namespace DSM


#endif
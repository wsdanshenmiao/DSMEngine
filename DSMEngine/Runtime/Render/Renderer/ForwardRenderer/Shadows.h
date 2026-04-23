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
        enum ShadowOption
        {
            None = 0,
            AlphaClip = 1 << 0,
            EnableDepthClip = 1 << 1,
            AllOptions = (1 << 2) - 1
        };
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

        void DrawModelShadow(IFramebuffer* framebuffer, const Math::Matrix4& viewProj, Viewport viewport, bool isDirectionalLightShadow);

        Viewport GetTileViewport(size_t index, size_t split, size_t tileSize) const;

        Math::Matrix4 ConvertToAtlasMatrix(const Math::Matrix4& m, Math::Vector2 offset, float scale) const;

        void ResizeShadowMap(IDevice* device);

        size_t GetCubeMapFaceIndex(const Math::Vector3& direction) const;
        Math::Vector3 GetCubeMapFaceDirection(size_t faceIndex) const;

        ShaderResource::OtherLightShadowData GetOtherLightShadowData(
            Math::Matrix4 viewProj,
            const Math::Vector2& offset, 
            float scale, 
            float border) const;

    private:
        IGraphicsPipeline* GetShadowPipeline(size_t index, ShadowSetting::FilterMode filter) const
        {
            return m_ShadowPipeline[index * ShadowSetting::FilterMode::Count + size_t(filter)].Get();
        }

    public:
        inline static ShadowSetting sm_Setting;
        
        inline static BufferHandle sm_ShadowCB{};
        inline static BufferHandle sm_DirectionalShadowMatrixBuffer{};
        inline static BufferHandle sm_OtherLightShadowDataBuffer{};

        static constexpr size_t sm_MaxShadowedDirectionalLightCount = 4;
        static constexpr size_t sm_MaxShadowedOtherLightCount = 16;

    private:
        using ShadowMatrixArray = std::array<Math::Matrix4, sm_MaxShadowedDirectionalLightCount * ShadowSetting::sm_MaxCascadeCount>;
        using PipelineArray = std::array<GraphicsPipelineHandle, (AllOptions + 1) * ShadowSetting::FilterMode::Count>;

        CommandListHandle m_CmdList;

        BufferHandle m_PassCB{};
        IBuffer* m_CacheMeshBuffer{nullptr};
        IBuffer* m_CacheMaterialBuffer{nullptr};

        ShadowMatrixArray m_DirectionalShadowMatrices{};
        std::array<ShaderResource::OtherLightShadowData, sm_MaxShadowedOtherLightCount> m_OtherLightShadowData{};
        std::vector<const Light*> m_ReservedDirectionalLights{};
        std::vector<const Light*> m_ReservedOtherLights{};
        size_t m_OtherLightShadowCount{0};

        FramebufferHandle m_DirectionalShadowFB;
        FramebufferHandle m_OtherShadowFB;

        PipelineArray m_ShadowPipeline{};
        BindingLayoutHandle m_ShadowBindingLayout;

        BindingSetHandle m_ShadowBindingSet;

        Math::Frustum m_CameraFrustum{};
    };
} // namespace DSM


#endif
#pragma once

#include <cstdint>

namespace DSM::RestirDI {

    inline constexpr uint32_t kReferenceSamplesPerPixel = 512u;

    enum class RenderMode : uint32_t
    {
        UnbiasedRestir = 0u,
        Reference = 1u
    };

    enum class DebugView : uint32_t
    {
        Final = 0u,
        Surface = 1u,
        Normal = 2u,
        Albedo = 3u,
        SourceType = 4u,
        SourceID = 5u,
        PHat = 6u,
        ReservoirM = 7u,
        SupportRatio = 8u,
        ReservoirW = 9u,
        TemporalAcceptance = 10u,
        SpatialAcceptance = 11u,
        Visibility = 12u
    };

    enum class EnvironmentSource : uint32_t
    {
        DaylightCube = 0u,
        RadianceHDR = 1u
    };

    // HLSL 端 RestirRenderMode/RestirDebugView 使用同一序号写入 GpuFrameConstants。
    // 显式断言可在调整菜单顺序时立刻暴露协议破坏，而不是等到 GPU 显示错误模式。
    static_assert(static_cast<uint32_t>(RenderMode::UnbiasedRestir) == 0u);
    static_assert(static_cast<uint32_t>(RenderMode::Reference) == 1u);
    static_assert(static_cast<uint32_t>(DebugView::Final) == 0u);
    static_assert(static_cast<uint32_t>(DebugView::Visibility) == 12u);

    struct Settings
    {
        RenderMode renderMode = RenderMode::UnbiasedRestir;
        DebugView debugView = DebugView::Final;

        uint32_t initialCandidateCount = 32;
        uint32_t temporalHistoryMCapMultiplier = 20;
        uint32_t spatialNeighborCount = 3;
        float spatialRadius = 30.0f;
        float temporalNormalThresholdDegrees = 25.0f;
        float temporalRelativeDepthThreshold = 0.1f;

        bool enableTemporalReuse = true;
        bool enableSpatialReuse = true;
        bool enableAnalyticLights = true;
        bool enableEmissiveTriangles = true;
        bool enableEnvironment = true;
        bool freezeRandomSeed = false;
        bool enableCameraControl = true;

        float analyticDomainWeight = 1.0f;
        float emissiveDomainWeight = 1.0f;
        float environmentDomainWeight = 1.0f;
        float alphaCutoff = 0.5f;
        float normalBias = 0.001f;
        float maxRayDistance = 10000.0f;
        float environmentIntensity = 1.0f;
        float environmentRotationDegrees = 0.0f;
        float exposure = 0.0f;
        float cameraMoveSpeed = 5.0f;
        float cameraMouseSensitivity = 0.005f;
        uint32_t randomSeed = 0x9E3779B9u;
    };

}

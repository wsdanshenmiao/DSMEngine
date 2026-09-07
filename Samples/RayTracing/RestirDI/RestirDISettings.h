#pragma once

#include <cstdint>

namespace DSM::RestirDI {

    enum class RenderMode : uint32_t
    {
        Restir = 0u,
        IndependentRIS = 1u,
        Reference = 2u
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
        ReservoirW = 8u,
        TemporalAcceptance = 9u,
        SpatialAcceptance = 10u,
        Visibility = 11u
    };

    enum class EnvironmentSource : uint32_t
    {
        DaylightCube = 0u,
        RadianceHDR = 1u
    };

    // HLSL 端 RestirRenderMode/RestirDebugView 使用同一序号写入 GpuFrameConstants。
    // 显式断言可在调整菜单顺序时立刻暴露协议破坏，而不是等到 GPU 显示错误模式。
    static_assert(static_cast<uint32_t>(RenderMode::Restir) == 0u);
    static_assert(static_cast<uint32_t>(RenderMode::IndependentRIS) == 1u);
    static_assert(static_cast<uint32_t>(RenderMode::Reference) == 2u);
    static_assert(static_cast<uint32_t>(DebugView::Final) == 0u);
    static_assert(static_cast<uint32_t>(DebugView::Visibility) == 11u);

    struct Settings
    {
        RenderMode renderMode = RenderMode::Restir;
        DebugView debugView = DebugView::Final;

        uint32_t initialCandidateCount = 32;
        uint32_t samplesPerPixel = 1;
        uint32_t referenceSamplesPerPixel = 256;
        uint32_t historyMCapMultiplier = 20;
        uint32_t spatialPassCount = 2;
        uint32_t spatialNeighborCount = 5;
        float spatialRadius = 30.0f;
        float normalThresholdDegrees = 25.0f;
        float relativeDepthThreshold = 0.1f;

        bool enableTemporalReuse = true;
        bool enableSpatialReuse = true;
        // 原论文 Algorithm 5 可在初始 RIS 前复用上一帧可见性；当前实现只对
        // 最终 Reservoir 样本追踪一次 DXR 阴影射线，因此该开关作为保留字段，
        // 不应被误认为已经启用了“初始可见性复用”。
        bool enableVisibilityReuse = false;
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

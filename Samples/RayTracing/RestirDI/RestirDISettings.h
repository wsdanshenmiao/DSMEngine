#pragma once

#include <cstdint>

namespace DSM::RestirDI {

    enum class RenderMode : uint32_t
    {
        Restir,
        IndependentRIS,
        Reference
    };

    enum class DebugView : uint32_t
    {
        Final,
        Surface,
        Normal,
        Albedo,
        SourceType,
        SourceID,
        PHat,
        ReservoirM,
        ReservoirW,
        TemporalAcceptance,
        SpatialAcceptance,
        Visibility
    };

    enum class EnvironmentSource : uint32_t
    {
        DaylightCube,
        RadianceHDR
    };

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

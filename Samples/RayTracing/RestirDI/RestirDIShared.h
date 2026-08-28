#pragma once

#include "Runtime/Math/MathCommon.h"

#include <cstddef>
#include <cstdint>

namespace DSM::RestirDI {

    inline constexpr uint32_t kPrimaryInstanceMask = 1u;
    inline constexpr uint32_t kShadowInstanceMask = 2u;
    inline constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    enum class SourceType : uint32_t
    {
        Invalid,
        Analytic,
        EmissiveTriangle,
        Environment
    };

    struct alignas(16) GpuFloat4
    {
        float x{}, y{}, z{}, w{};
    };

    struct alignas(16) GpuUint4
    {
        uint32_t x{}, y{}, z{}, w{};
    };

    struct alignas(16) GpuMatrix
    {
        GpuFloat4 row0{};
        GpuFloat4 row1{};
        GpuFloat4 row2{};
        GpuFloat4 row3{};
    };

    struct alignas(16) GpuVertex
    {
        GpuFloat4 position{};
        GpuFloat4 normal{};
        GpuFloat4 tangent{};
        GpuFloat4 uv{};
    };

    struct alignas(16) GpuGeometry
    {
        // vertexBase、indexOffset、indexCount、materialIndex
        GpuUint4 data{};
    };

    struct alignas(16) GpuInstance
    {
        GpuMatrix currentLocalToWorld{};
        GpuMatrix previousLocalToWorld{};
        // stableID、geometryBase、geometryCount、flags
        GpuUint4 data{};
    };

    struct alignas(16) GpuMaterial
    {
        GpuFloat4 baseColor{};
        GpuFloat4 emissiveColor{};
        // normalScale、metallic、roughness、alphaCutoff
        GpuFloat4 factors{};
        // baseColor、roughness、metalness、normal
        GpuUint4 texture0{};
        // emissive、occlusion、flags、保留
        GpuUint4 texture1{};
    };

    struct alignas(16) GpuAnalyticLight
    {
        GpuFloat4 color{};
        // xyz 为位置，w 为 1/range。
        GpuFloat4 positionInvRange{};
        // xyz 为表面指向光源的方向，w 为 LightType。
        GpuFloat4 directionType{};
        // innerAngle、outerAngle、功率、保留。
        GpuFloat4 anglesPower{};
        // stableID、保留。
        GpuUint4 metadata{};
    };

    struct alignas(16) GpuAliasEntry
    {
        float probability = 1.0f;
        uint32_t alias = 0;
        float pmf = 1.0f;
        uint32_t padding = 0;
    };

    struct alignas(16) GpuEmissiveTriangle
    {
        // instanceIndex、indexOffset、materialIndex、stableID。
        GpuUint4 data{};
        // worldArea、功率、保留。
        GpuFloat4 areaPower{};
    };

    struct alignas(16) GpuSurface
    {
        GpuFloat4 positionDepth{};
        GpuFloat4 normalRoughness{};
        GpuFloat4 albedoMetallic{};
        GpuFloat4 emissive{};
        // previousUV.xy、currentDeviceDepth、previousDeviceDepth。
        GpuFloat4 motion{};
        // stableID、instanceIndex、materialIndex、flags。
        GpuUint4 ids{};
    };

    struct alignas(16) GpuReservoirSample
    {
        uint32_t sourceType = 0;
        uint32_t stableID = kInvalidIndex;
        uint32_t itemIndex = kInvalidIndex;
        uint32_t sampleSeed = 0;
    };

    struct alignas(16) GpuReservoirStats
    {
        float weightSum = 0.0f;
        float M = 0.0f;
        float W = 0.0f;
        float selectedPHat = 0.0f;
    };

    struct alignas(16) GpuAcceptance
    {
        uint32_t temporalAccepted = 0;
        uint32_t spatialAccepted = 0;
        uint32_t spatialRejected = 0;
        uint32_t visibility = 0;
    };

    struct alignas(16) GpuFrameConstants
    {
        GpuMatrix viewProjection{};
        GpuMatrix inverseViewProjection{};
        GpuMatrix previousViewProjection{};
        // xyz 为相机位置，w 为曝光 EV。
        GpuFloat4 cameraExposure{};
        // normalCos、relativeDepth、spatialRadius、normalBias。
        GpuFloat4 reuseThresholds{};
        // 解析灯、自发光、环境域概率与环境强度。
        GpuFloat4 domainProbabilities{};
        // 环境旋转、最大光线距离、Alpha Cutoff、历史是否有效。
        GpuFloat4 rayEnvironment{};
        // width、height、frameIndex、randomSeed。
        GpuUint4 resolutionFrame{};
        // lightCount、emissiveCount、environmentCount、instanceCount。
        GpuUint4 sourceCounts{};
        // initialCandidates、historyMCap、spatialNeighbors、spatialPassIndex。
        GpuUint4 algorithm{};
        // renderMode、debugView、temporalEnabled、spatialEnabled。
        GpuUint4 modes{};
        // environmentWidth、environmentHeight、候选域启用掩码、保留。
        GpuUint4 environmentInfo{};
        // ReSTIR/Independent RIS SPP、Reference SPP、当前样本 lane、保留。
        GpuUint4 sampling{};
    };

    [[nodiscard]] inline GpuFloat4 ToGpuFloat4(const Math::Vector4& value) noexcept
    {
        return {value.Get(0), value.Get(1), value.Get(2), value.Get(3)};
    }

    [[nodiscard]] inline GpuFloat4 ToGpuFloat4(const Math::Vector3& value, float w = 0.0f) noexcept
    {
        return {value.Get(0), value.Get(1), value.Get(2), w};
    }

    [[nodiscard]] inline GpuMatrix ToGpuMatrix(const Math::Matrix4& value) noexcept
    {
        GpuMatrix result{};
        GpuFloat4* rows[] = {&result.row0, &result.row1, &result.row2, &result.row3};
        for (size_t row = 0; row < 4; ++row) {
            *rows[row] = {
                value.Get(row, 0), value.Get(row, 1),
                value.Get(row, 2), value.Get(row, 3)};
        }
        return result;
    }

    static_assert(sizeof(GpuFloat4) == 16);
    static_assert(sizeof(GpuUint4) == 16);
    static_assert(sizeof(GpuMatrix) == 64);
    static_assert(sizeof(GpuVertex) == 64);
    static_assert(sizeof(GpuGeometry) == 16);
    static_assert(sizeof(GpuInstance) == 144);
    static_assert(sizeof(GpuMaterial) == 80);
    static_assert(sizeof(GpuAnalyticLight) == 80);
    static_assert(sizeof(GpuAliasEntry) == 16);
    static_assert(sizeof(GpuEmissiveTriangle) == 32);
    static_assert(sizeof(GpuSurface) == 96);
    static_assert(sizeof(GpuReservoirSample) == 16);
    static_assert(sizeof(GpuReservoirStats) == 16);
    static_assert(sizeof(GpuAcceptance) == 16);
    static_assert(sizeof(GpuFrameConstants) == 352);
    static_assert(offsetof(GpuReservoirStats, W) == 8);
    static_assert(offsetof(GpuFrameConstants, resolutionFrame) == 256);
    static_assert(offsetof(GpuFrameConstants, environmentInfo) == 320);
    static_assert(offsetof(GpuFrameConstants, sampling) == 336);

}

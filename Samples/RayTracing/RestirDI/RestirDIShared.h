#pragma once

#include "Runtime/Math/MathCommon.h"

#include <cstddef>
#include <cstdint>

namespace DSM::RestirDI {

    // 必须与 Shaders/RestirDICommon.hlsli 中的 RestirInstanceMask 保持数值一致。
    // 主射线看到所有可见实例；阴影射线只看到允许投射阴影的实例。
    inline constexpr uint32_t kPrimaryInstanceMask = 1u;
    inline constexpr uint32_t kShadowInstanceMask = 2u;
    inline constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    enum class SourceType : uint32_t
    {
        Invalid = 0u,
        Analytic = 1u,
        EmissiveTriangle = 2u,
        Environment = 3u
    };

    // 这些值写入 GpuReservoirSample.sourceType，并在 HLSL 中用于选择 q 域。
    // 论文第 3.1 节的单一候选分布在本项目中扩展为三域混合分布：解析灯、
    // 自发光三角形和环境贴图；Invalid 表示空 Reservoir。
    static_assert(static_cast<uint32_t>(SourceType::Invalid) == 0u);
    static_assert(static_cast<uint32_t>(SourceType::Analytic) == 1u);
    static_assert(static_cast<uint32_t>(SourceType::EmissiveTriangle) == 2u);
    static_assert(static_cast<uint32_t>(SourceType::Environment) == 3u);

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
        // 不直接跨 ABI 传 Math::Matrix4/float4x4：DSMEngine 使用行向量语义，
        // 显式四行可固定 StructuredBuffer 的 64 字节布局并避免 HLSL 默认
        // column_major 与隐式转置。HLSL 的 MulRow 与此上传顺序成对使用。
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
        // Alias 列内选择当前列的条件概率；它不是候选最终的离散概率。
        float probability = 1.0f;
        // 未选择当前列时改为选择的候选索引。
        uint32_t alias = 0;
        // 此候选真正的离散 PMF。计算 ReSTIR proposal PDF q 时必须使用该值。
        float pmf = 1.0f;
        // 保证 C++ 与 HLSL StructuredBuffer 元素均为 16 字节。
        uint32_t padding = 0;
    };

    struct alignas(16) GpuEmissiveTriangle
    {
        // instanceIndex、indexOffset、materialIndex、stableID。
        GpuUint4 data{};
        // worldArea、近似功率（worldArea * emissiveLuminance）、保留。
        // worldArea 用于把三角形离散 PMF 转换为面积测度 PDF；功率用于 CPU 构建 Alias Table。
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
        // sample 只描述 Algorithm 2 选出的候选 y；统计量单独存储。
        uint32_t sourceType = static_cast<uint32_t>(SourceType::Invalid);
        uint32_t stableID = kInvalidIndex;
        uint32_t itemIndex = kInvalidIndex;
        uint32_t sampleSeed = 0;
    };

    struct alignas(16) GpuReservoirStats
    {
        // weightSum = Σw_i，M = 输入流包含的候选数。
        // normalizationM 是 Algorithm 6 的支持质量 Z；初始 RIS 中 Z=M，
        // 无偏时空合并中只累计对最终样本 y 满足 pHat_qi(y)>0 的来源 M。
        float weightSum = 0.0f;
        float M = 0.0f;
        float W = 0.0f;
        float normalizationM = 0.0f;
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
        // 解析灯、自发光、环境域概率与环境强度。前三项是混合 proposal 的
        // domain PDF；候选域内的 Alias PMF 会在 EvaluateCandidate 中相乘。
        GpuFloat4 domainProbabilities{};
        // 环境旋转、最大光线距离、Alpha Cutoff、历史是否有效。
        GpuFloat4 rayEnvironment{};
        // width、height、frameIndex、randomSeed。
        GpuUint4 resolutionFrame{};
        // lightCount、emissiveCount、environmentCount、instanceCount。
        GpuUint4 sourceCounts{};
        // initialCandidates、temporalHistoryMCapMultiplier、spatialNeighbors、Reference SPP。
        // 上一帧 M 最多取当前 M 的 multiplier 倍，与论文第 5 节一致。
        GpuUint4 algorithm{};
        // renderMode、debugView、temporalEnabled、spatialEnabled。
        GpuUint4 modes{};
        // environmentWidth、environmentHeight、候选域启用掩码、保留。
        GpuUint4 environmentInfo{};
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
    static_assert(sizeof(GpuFrameConstants) == 336);
    static_assert(offsetof(GpuReservoirStats, W) == 8);
    static_assert(offsetof(GpuReservoirStats, normalizationM) == 12);
    static_assert(offsetof(GpuFrameConstants, resolutionFrame) == 256);
    static_assert(offsetof(GpuFrameConstants, environmentInfo) == 320);

}

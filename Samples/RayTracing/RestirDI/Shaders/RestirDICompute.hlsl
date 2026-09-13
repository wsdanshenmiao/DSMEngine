#include "RestirDICommon.hlsli"

bool PixelInBounds(uint2 pixel)
{
    return pixel.x < g_Frame.resolutionFrame.x && pixel.y < g_Frame.resolutionFrame.y;
}

[numthreads(8, 8, 1)]
void InitialRISCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (!PixelInBounds(pixel))
        return;
    uint index = pixel.y * g_Frame.resolutionFrame.x + pixel.x;
    GpuSurface surface = g_SurfaceCurrent[index];
    GpuReservoirSample reservoirSample;
    GpuReservoirStats reservoirStats;
    ReservoirClear(reservoirSample, reservoirStats);
    GpuAcceptance acceptance = (GpuAcceptance)0;

    if ((surface.ids.w & ValidSurface) != 0u) {
        uint randomState = Hash(index ^ g_Frame.resolutionFrame.w ^
            Hash(g_Frame.resolutionFrame.z + 0x68bc21ebu));
        uint candidateCount = max(g_Frame.algorithm.x, 1u);
        [loop] for (uint candidateIndex = 0u; candidateIndex < candidateCount; ++candidateIndex) {
            GpuReservoirSample candidate = GenerateCandidate(randomState);
            CandidateEvaluation evaluation = EvaluateCandidate(surface, candidate);
            const bool validProposal = HasValidProposalPdf(evaluation);
            float pHat = validProposal ? Luminance(evaluation.contribution) : 0.0f;
            float weight = validProposal && isfinite(pHat)
                ? pHat / evaluation.proposalPdf : 0.0f;
            // 论文 Algorithm 3：每个候选贡献一个 M=1 的 RIS 样本，
            // ReservoirUpdate 内部执行 w=pHat/q 的加权替换。
            ReservoirUpdate(reservoirSample, reservoirStats, candidate, weight, 1.0f, randomState);
        }
        // 单一初始流对自身全部候选都有相同来源，因此 Algorithm 6 的 Z=M。
        ReservoirFinalize(surface, reservoirSample, reservoirStats, reservoirStats.M);
    }

    g_ReservoirSampleOutput[index] = reservoirSample;
    g_ReservoirStatsOutput[index] = reservoirStats;
    g_AcceptanceOutput[index] = acceptance;
}

[numthreads(8, 8, 1)]
void TemporalReuseCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (!PixelInBounds(pixel)) return;
    uint width = g_Frame.resolutionFrame.x;
    uint height = g_Frame.resolutionFrame.y;
    uint index = pixel.y * width + pixel.x;
    GpuSurface surface = g_SurfaceCurrent[index];
    GpuReservoirSample currentSample = g_ReservoirCurrentSample[index];
    GpuReservoirStats currentStats = g_ReservoirCurrentStats[index];
    GpuReservoirSample outputSample;
    GpuReservoirStats outputStats;
    ReservoirClear(outputSample, outputStats);
    GpuAcceptance acceptance = g_AcceptanceInput[index];
    uint selectionState = Hash(index ^ g_Frame.resolutionFrame.w ^
        g_Frame.resolutionFrame.z ^ 0x967a889bu);

    if ((surface.ids.w & ValidSurface) == 0u) {
        g_ReservoirSampleOutput[index] = outputSample;
        g_ReservoirStatsOutput[index] = outputStats;
        g_AcceptanceOutput[index] = acceptance;
        return;
    }

    // 当前 RIS 也必须作为原子来源插入空 Reservoir。直接继承它的 weightSum
    // 只适用于以总 M 归一化的有偏 Algorithm 4。
    const float currentM = isfinite(currentStats.M) ? max(currentStats.M, 0.0f) : 0.0f;
    ReservoirMergeSource(surface, currentSample, currentStats, currentM,
        outputSample, outputStats, selectionState);

    bool historyIncluded = false;
    GpuSurface historySurface = (GpuSurface)0;
    GpuReservoirSample historySample = (GpuReservoirSample)0;
    GpuReservoirStats historyStats = (GpuReservoirStats)0;
    float historyM = 0.0f;

    if (g_Frame.rayEnvironment.w > 0.5f) {
        float2 previousPixelPosition = surface.motion.xy * float2(width, height) - 0.5f;
        int2 previousPixel = int2(round(previousPixelPosition));
        if (all(previousPixel >= 0) && previousPixel.x < (int)width && previousPixel.y < (int)height) {
            uint previousIndex = previousPixel.y * width + previousPixel.x;
            historySurface = g_SurfacePrevious[previousIndex];
            historySample = g_ReservoirHistorySample[previousIndex];
            historyStats = g_ReservoirHistoryStats[previousIndex];
            if (TemporalSurfaceCompatible(surface, historySurface) &&
                historyStats.M > 0.0f && isfinite(historyStats.M)) {
                // 论文第 5 节：只限制上一帧来源 M，不在 spatial pass 后全局裁剪。
                const float historyCap = currentM * max((float)g_Frame.algorithm.y, 1.0f);
                historyM = min(historyStats.M, historyCap);
                ReservoirMergeSource(surface, historySample, historyStats, historyM,
                    outputSample, outputStats, selectionState);
                historyIncluded = historyM > 0.0f;
                acceptance.temporalAccepted = historyIncluded ? 1u : 0u;
            }
        }
    }

    // Algorithm 6 第 6–10 行：对最终选中的同一个 y，在每个来源表面重新
    // 评价 target 支持域。未启用 visibility reuse，故这里不需要额外阴影射线。
    float normalizationM = ReservoirSupportM(surface, outputSample, currentM);
    if (historyIncluded) {
        normalizationM += ReservoirSupportM(historySurface, outputSample, historyM);
    }
    ReservoirFinalize(surface, outputSample, outputStats, normalizationM);

    g_ReservoirSampleOutput[index] = outputSample;
    g_ReservoirStatsOutput[index] = outputStats;
    g_AcceptanceOutput[index] = acceptance;
}

int2 SampleSpatialOffset(inout uint neighborState)
{
    const float angle = RandomFloat(neighborState) * (2.0f * RESTIR_PI);
    const float radius = sqrt(RandomFloat(neighborState)) * g_Frame.reuseThresholds.z;
    return int2(round(float2(cos(angle), sin(angle)) * radius));
}

bool ResolveSpatialSource(
    uint2 centerPixel,
    uint width,
    uint height,
    int2 offset,
    out uint sourceIndex,
    out GpuSurface sourceSurface,
    out GpuReservoirSample sourceSample,
    out GpuReservoirStats sourceStats)
{
    sourceIndex = 0u;
    sourceSurface = (GpuSurface)0;
    sourceSample = (GpuReservoirSample)0;
    sourceStats = (GpuReservoirStats)0;
    const int2 sourcePixel = int2(centerPixel) + offset;
    if (all(offset == 0) || any(sourcePixel < 0) ||
        sourcePixel.x >= (int)width || sourcePixel.y >= (int)height) return false;
    sourceIndex = sourcePixel.y * width + sourcePixel.x;
    sourceSurface = g_SurfaceCurrent[sourceIndex];
    sourceSample = g_ReservoirCurrentSample[sourceIndex];
    sourceStats = g_ReservoirCurrentStats[sourceIndex];
    return (sourceSurface.ids.w & ValidSurface) != 0u &&
        sourceStats.M > 0.0f && isfinite(sourceStats.M);
}

[numthreads(8, 8, 1)]
void SpatialReuseCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (!PixelInBounds(pixel)) return;
    uint width = g_Frame.resolutionFrame.x;
    uint height = g_Frame.resolutionFrame.y;
    uint index = pixel.y * width + pixel.x;
    GpuSurface surface = g_SurfaceCurrent[index];
    GpuReservoirSample centerSample = g_ReservoirCurrentSample[index];
    GpuReservoirStats centerStats = g_ReservoirCurrentStats[index];
    GpuReservoirSample outputSample;
    GpuReservoirStats outputStats;
    ReservoirClear(outputSample, outputStats);
    GpuAcceptance acceptance = g_AcceptanceInput[index];
    if ((surface.ids.w & ValidSurface) == 0u) {
        g_ReservoirSampleOutput[index] = outputSample;
        g_ReservoirStatsOutput[index] = outputStats;
        g_AcceptanceOutput[index] = acceptance;
        return;
    }

    const uint neighborSeed = Hash(index ^ g_Frame.resolutionFrame.w ^
        Hash(g_Frame.resolutionFrame.z * 17u) ^ 0x368cc8b7u);
    uint neighborState = neighborSeed;
    uint selectionState = Hash(neighborSeed ^ 0xa511e9b3u);
    const uint neighborCount = g_Frame.algorithm.z;
    const float centerM = isfinite(centerStats.M) ? max(centerStats.M, 0.0f) : 0.0f;

    // Algorithm 6 第一遍：中心与随机邻居均作为原子来源，从空 Reservoir 合并。
    ReservoirMergeSource(surface, centerSample, centerStats, centerM,
        outputSample, outputStats, selectionState);

    [loop] for (uint neighborIndex = 0u; neighborIndex < neighborCount; ++neighborIndex) {
        const int2 offset = SampleSpatialOffset(neighborState);
        uint sourceIndex;
        GpuSurface sourceSurface;
        GpuReservoirSample sourceSample;
        GpuReservoirStats sourceStats;
        if (!ResolveSpatialSource(pixel, width, height, offset,
            sourceIndex, sourceSurface, sourceSample, sourceStats)) {
            acceptance.spatialRejected++;
            continue;
        }
        ReservoirMergeSource(surface, sourceSample, sourceStats, sourceStats.M,
            outputSample, outputStats, selectionState);
        acceptance.spatialAccepted++;
    }

    // Algorithm 6 第二遍：重放完全相同的邻居序列，对最终 y 求支持质量 Z。
    float normalizationM = ReservoirSupportM(surface, outputSample, centerM);
    neighborState = neighborSeed;
    [loop] for (uint supportIndex = 0u; supportIndex < neighborCount; ++supportIndex) {
        const int2 offset = SampleSpatialOffset(neighborState);
        uint sourceIndex;
        GpuSurface sourceSurface;
        GpuReservoirSample sourceSample;
        GpuReservoirStats sourceStats;
        if (!ResolveSpatialSource(pixel, width, height, offset,
            sourceIndex, sourceSurface, sourceSample, sourceStats)) continue;
        normalizationM += ReservoirSupportM(sourceSurface, outputSample, sourceStats.M);
    }
    ReservoirFinalize(surface, outputSample, outputStats, normalizationM);
    g_ReservoirSampleOutput[index] = outputSample;
    g_ReservoirStatsOutput[index] = outputStats;
    g_AcceptanceOutput[index] = acceptance;
}

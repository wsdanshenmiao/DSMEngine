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
            Hash(g_Frame.resolutionFrame.z + 0x68bc21ebu) ^
            Hash(g_Frame.sampling.z + 0x02e5be93u));
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
            ReservoirUpdate(reservoirSample, reservoirStats, candidate, pHat, weight, 1.0f, randomState);
        }
        ReservoirFinalize(reservoirSample, reservoirStats);
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
    GpuReservoirSample outputSample = g_ReservoirCurrentSample[index];
    GpuReservoirStats outputStats = g_ReservoirCurrentStats[index];
    GpuAcceptance acceptance = g_AcceptanceInput[index];

    // 论文 Algorithm 5 的 temporal reuse；当前项目把上一帧 Reservoir 当作
    // 一个拥有 historyStats.M 个候选的输入 Reservoir，再按 Algorithm 4 合并。
    if (g_Frame.rayEnvironment.w > 0.5f && (surface.ids.w & ValidSurface) != 0u) {
        float2 previousPixelPosition = surface.motion.xy * float2(width, height) - 0.5f;
        int2 previousPixel = int2(round(previousPixelPosition));
        if (all(previousPixel >= 0) && previousPixel.x < (int)width && previousPixel.y < (int)height) {
            uint previousIndex = previousPixel.y * width + previousPixel.x;
            GpuSurface previousSurface = g_SurfacePrevious[previousIndex];
            GpuReservoirSample historySample = g_ReservoirHistorySample[previousIndex];
            GpuReservoirStats historyStats = g_ReservoirHistoryStats[previousIndex];
            if (SurfaceCompatible(surface, previousSurface) && historySample.sourceType != InvalidSource && historyStats.W > 0.0f) {
                CandidateEvaluation evaluation = EvaluateCandidate(surface, historySample);
                if (HasValidProposalPdf(evaluation)) {
                    float currentPHat = Luminance(evaluation.contribution);
                    float sourceM = min(historyStats.M, (float)g_Frame.algorithm.y);
                    // Algorithm 4 的合并权重：pHat_q(y) * W_source * M_source。
                    // 重新评价历史样本是必要的；直接沿用历史 pHat 会在表面或
                    // proposal 改变后产生错误的 selection probability。
                    float mergeWeight = currentPHat * historyStats.W * sourceM;
                    uint randomState = Hash(index ^ historySample.sampleSeed ^
                        g_Frame.resolutionFrame.z ^ Hash(g_Frame.sampling.z + 0x967a889bu));
                    ReservoirUpdate(outputSample, outputStats, historySample,
                        currentPHat, mergeWeight, sourceM, randomState);
                    ReservoirFinalize(outputSample, outputStats);
                    ReservoirApplyMCap(outputStats, (float)g_Frame.algorithm.y);
                    acceptance.temporalAccepted = sourceM > 0.0f ? 1u : 0u;
                }
            }
        }
    }

    g_ReservoirSampleOutput[index] = outputSample;
    g_ReservoirStatsOutput[index] = outputStats;
    g_AcceptanceOutput[index] = acceptance;
}

bool SpatialSurfaceCompatible(GpuSurface center, GpuSurface neighbor)
{
    if ((center.ids.w & ValidSurface) == 0u || (neighbor.ids.w & ValidSurface) == 0u) return false;
    if (center.ids.z != neighbor.ids.z) return false;
    float normalSimilarity = dot(
        SafeNormalize(center.normalRoughness.xyz, float3(0, 1, 0)),
        SafeNormalize(neighbor.normalRoughness.xyz, float3(0, 1, 0)));
    float relativeDepth = abs(center.motion.z - neighbor.motion.z) / max(abs(center.motion.z), 1e-3f);
    return normalSimilarity >= g_Frame.reuseThresholds.x && relativeDepth <= g_Frame.reuseThresholds.y;
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
    GpuReservoirSample outputSample = g_ReservoirCurrentSample[index];
    GpuReservoirStats outputStats = g_ReservoirCurrentStats[index];
    GpuAcceptance acceptance = g_AcceptanceInput[index];
    uint randomState = Hash(index ^ g_Frame.resolutionFrame.w ^
        Hash(g_Frame.algorithm.w + g_Frame.resolutionFrame.z * 17u) ^
        Hash(g_Frame.sampling.z + 0x368cc8b7u));
    uint neighborCount = g_Frame.algorithm.z;

    [loop] for (uint neighborIndex = 0u; neighborIndex < neighborCount; ++neighborIndex) {
        float angle = RandomFloat(randomState) * (2.0f * RESTIR_PI);
        float radius = sqrt(RandomFloat(randomState)) * g_Frame.reuseThresholds.z;
        int2 offset = int2(round(float2(cos(angle), sin(angle)) * radius));
        int2 neighborPixel = int2(pixel) + offset;
        if (all(offset == 0) || any(neighborPixel < 0) ||
            neighborPixel.x >= (int)width || neighborPixel.y >= (int)height) {
            acceptance.spatialRejected++;
            continue;
        }
        uint sourceIndex = neighborPixel.y * width + neighborPixel.x;
        GpuSurface neighborSurface = g_SurfaceCurrent[sourceIndex];
        GpuReservoirSample neighborSample = g_ReservoirCurrentSample[sourceIndex];
        GpuReservoirStats neighborStats = g_ReservoirCurrentStats[sourceIndex];
        // 论文 Algorithm 5 的空间 pass；每次只把邻域 Reservoir 的“代表样本”
        // 重新评价到中心表面，并用 pHat_current * W_neighbor * M_neighbor 合并。
        if (!SpatialSurfaceCompatible(surface, neighborSurface) ||
            neighborSample.sourceType == InvalidSource || !(neighborStats.W > 0.0f)) {
            acceptance.spatialRejected++;
            continue;
        }
        CandidateEvaluation evaluation = EvaluateCandidate(surface, neighborSample);
        if (HasValidProposalPdf(evaluation)) {
            float currentPHat = Luminance(evaluation.contribution);
            float sourceM = min(neighborStats.M, (float)g_Frame.algorithm.y);
            // 与 temporal 合并相同，邻居 Reservoir 的 W/M 先还原其代表的
            // 候选质量，再以中心表面的 currentPHat 作为目标函数。
            float mergeWeight = currentPHat * neighborStats.W * sourceM;
            ReservoirUpdate(outputSample, outputStats, neighborSample,
                currentPHat, mergeWeight, sourceM, randomState);
            acceptance.spatialAccepted += sourceM > 0.0f ? 1u : 0u;
        }
        else {
            acceptance.spatialRejected++;
        }
    }
    ReservoirFinalize(outputSample, outputStats);
    ReservoirApplyMCap(outputStats, (float)g_Frame.algorithm.y);
    g_ReservoirSampleOutput[index] = outputSample;
    g_ReservoirStatsOutput[index] = outputStats;
    g_AcceptanceOutput[index] = acceptance;
}

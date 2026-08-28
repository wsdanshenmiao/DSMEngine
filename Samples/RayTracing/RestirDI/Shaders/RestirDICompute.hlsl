#include "RestirDICommon.hlsli"

bool PixelInBounds(uint2 pixel)
{
    return pixel.x < g_Frame.resolutionFrame.x && pixel.y < g_Frame.resolutionFrame.y;
}

[numthreads(8, 8, 1)]
void InitialRISCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (!PixelInBounds(pixel)) return;
    uint index = pixel.y * g_Frame.resolutionFrame.x + pixel.x;
    GpuSurface surface = g_SurfaceCurrent[index];
    GpuReservoirSample reservoirSample;
    GpuReservoirStats reservoirStats;
    ReservoirClear(reservoirSample, reservoirStats);
    GpuAcceptance acceptance = (GpuAcceptance)0;

    if ((surface.ids.w & 1u) != 0u) {
        uint randomState = Hash(index ^ g_Frame.resolutionFrame.w ^
            Hash(g_Frame.resolutionFrame.z + 0x68bc21ebu) ^
            Hash(g_Frame.sampling.z + 0x02e5be93u));
        uint candidateCount = max(g_Frame.algorithm.x, 1u);
        [loop] for (uint candidateIndex = 0u; candidateIndex < candidateCount; ++candidateIndex) {
            GpuReservoirSample candidate = GenerateCandidate(randomState);
            CandidateEvaluation evaluation = EvaluateCandidate(surface, candidate);
            float pHat = Luminance(evaluation.contribution);
            float weight = pHat / max(evaluation.proposalPdf, RESTIR_EPSILON);
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

    if (g_Frame.rayEnvironment.w > 0.5f && (surface.ids.w & 1u) != 0u) {
        float2 previousPixelPosition = surface.motion.xy * float2(width, height) - 0.5f;
        int2 previousPixel = int2(round(previousPixelPosition));
        if (all(previousPixel >= 0) && previousPixel.x < (int)width && previousPixel.y < (int)height) {
            uint previousIndex = previousPixel.y * width + previousPixel.x;
            GpuSurface previousSurface = g_SurfacePrevious[previousIndex];
            GpuReservoirSample historySample = g_ReservoirHistorySample[previousIndex];
            GpuReservoirStats historyStats = g_ReservoirHistoryStats[previousIndex];
            if (SurfaceCompatible(surface, previousSurface) && historySample.sourceType != 0u && historyStats.W > 0.0f) {
                CandidateEvaluation evaluation = EvaluateCandidate(surface, historySample);
                float currentPHat = Luminance(evaluation.contribution);
                float maxM = max((float)g_Frame.algorithm.y, outputStats.M);
                float sourceM = min(historyStats.M, max(maxM - outputStats.M, 0.0f));
                float mergeWeight = currentPHat * historyStats.W * sourceM;
                uint randomState = Hash(index ^ historySample.sampleSeed ^
                    g_Frame.resolutionFrame.z ^ Hash(g_Frame.sampling.z + 0x967a889bu));
                ReservoirUpdate(outputSample, outputStats, historySample,
                    currentPHat, mergeWeight, sourceM, randomState);
                ReservoirFinalize(outputSample, outputStats);
                acceptance.temporalAccepted = 1u;
            }
        }
    }

    g_ReservoirSampleOutput[index] = outputSample;
    g_ReservoirStatsOutput[index] = outputStats;
    g_AcceptanceOutput[index] = acceptance;
}

bool SpatialSurfaceCompatible(GpuSurface center, GpuSurface neighbor)
{
    if ((center.ids.w & 1u) == 0u || (neighbor.ids.w & 1u) == 0u) return false;
    float normalSimilarity = dot(normalize(center.normalRoughness.xyz), normalize(neighbor.normalRoughness.xyz));
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
        if (!SpatialSurfaceCompatible(surface, neighborSurface) ||
            neighborSample.sourceType == 0u || !(neighborStats.W > 0.0f)) {
            acceptance.spatialRejected++;
            continue;
        }
        CandidateEvaluation evaluation = EvaluateCandidate(surface, neighborSample);
        float currentPHat = Luminance(evaluation.contribution);
        float maxM = max((float)g_Frame.algorithm.y, outputStats.M);
        float sourceM = min(neighborStats.M, max(maxM - outputStats.M, 0.0f));
        float mergeWeight = currentPHat * neighborStats.W * sourceM;
        ReservoirUpdate(outputSample, outputStats, neighborSample,
            currentPHat, mergeWeight, sourceM, randomState);
        acceptance.spatialAccepted++;
    }
    ReservoirFinalize(outputSample, outputStats);
    g_ReservoirSampleOutput[index] = outputSample;
    g_ReservoirStatsOutput[index] = outputStats;
    g_AcceptanceOutput[index] = acceptance;
}

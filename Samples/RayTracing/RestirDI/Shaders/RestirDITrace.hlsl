#include "RestirDICommon.hlsli"

RaytracingAccelerationStructure g_Scene : register(t0);

struct RayPayload
{
    uint rayType;
    uint value;
};

float3 CameraRayDirection(uint2 pixel)
{
    float2 uv = (float2(pixel) + 0.5f) / float2(g_Frame.resolutionFrame.xy);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 world = MulRow(float4(ndc, 1.0f, 1.0f), g_Frame.inverseViewProjection);
    world.xyz /= max(abs(world.w), RESTIR_EPSILON);
    return normalize(world.xyz - g_Frame.cameraExposure.xyz);
}

float2 ProjectUV(float3 worldPosition, GpuMatrix viewProjection, out float deviceDepth)
{
    float4 clip = MulRow(float4(worldPosition, 1.0f), viewProjection);
    float inverseW = 1.0f / max(abs(clip.w), RESTIR_EPSILON);
    float3 ndc = clip.xyz * inverseW;
    deviceDepth = ndc.z;
    return float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
}

[shader("raygeneration")]
void PrimaryRayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint index = pixel.y * g_Frame.resolutionFrame.x + pixel.x;
    float3 rayDirection = CameraRayDirection(pixel);
    GpuSurface surface = (GpuSurface)0;
    surface.emissive = float4(SampleEnvironment(rayDirection), 1.0f);
    surface.ids = uint4(RESTIR_INVALID_INDEX, RESTIR_INVALID_INDEX, RESTIR_INVALID_INDEX, 2u);
    g_SurfaceOutput[index] = surface;

    if (g_Frame.sourceCounts.w == 0u) return;
    RayDesc ray;
    ray.Origin = g_Frame.cameraExposure.xyz;
    ray.Direction = rayDirection;
    ray.TMin = g_Frame.reuseThresholds.w;
    ray.TMax = g_Frame.rayEnvironment.y;
    RayPayload payload = {0u, 0u};
    TraceRay(g_Scene, RAY_FLAG_NONE, 1u, 0u, 0u, 0u, ray, payload);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    if (payload.rayType == 0u) payload.value = 0u;
}

[shader("anyhit")]
void AlphaAnyHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    GpuInstance instance = g_Instances[InstanceID()];
    GpuGeometry geometry = g_Geometries[instance.data.y + GeometryIndex()];
    GpuMaterial material = g_Materials[geometry.data.w];
    if ((material.texture1.z & 1u) == 0u) return;
    uint triangleOffset = geometry.data.y + PrimitiveIndex() * 3u;
    GpuVertex v0 = g_Vertices[geometry.data.x + g_Indices[triangleOffset]];
    GpuVertex v1 = g_Vertices[geometry.data.x + g_Indices[triangleOffset + 1u]];
    GpuVertex v2 = g_Vertices[geometry.data.x + g_Indices[triangleOffset + 2u]];
    float3 bary = float3(1.0f - attributes.barycentrics.x - attributes.barycentrics.y,
        attributes.barycentrics.x, attributes.barycentrics.y);
    float2 uv = v0.uv.xy * bary.x + v1.uv.xy * bary.y + v2.uv.xy * bary.z;
    float alpha = material.baseColor.a *
        g_Textures[NonUniformResourceIndex(material.texture0.x)].SampleLevel(g_LinearSampler, uv, 0.0f).a;
    if (alpha < material.factors.w) IgnoreHit();
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    if (payload.rayType != 0u) {
        payload.value = 0u;
        return;
    }

    uint instanceIndex = InstanceID();
    GpuInstance instance = g_Instances[instanceIndex];
    GpuGeometry geometry = g_Geometries[instance.data.y + GeometryIndex()];
    GpuMaterial material = g_Materials[geometry.data.w];
    uint triangleOffset = geometry.data.y + PrimitiveIndex() * 3u;
    GpuVertex v0 = g_Vertices[geometry.data.x + g_Indices[triangleOffset]];
    GpuVertex v1 = g_Vertices[geometry.data.x + g_Indices[triangleOffset + 1u]];
    GpuVertex v2 = g_Vertices[geometry.data.x + g_Indices[triangleOffset + 2u]];
    float3 bary = float3(1.0f - attributes.barycentrics.x - attributes.barycentrics.y,
        attributes.barycentrics.x, attributes.barycentrics.y);
    float3 localPosition = v0.position.xyz * bary.x + v1.position.xyz * bary.y + v2.position.xyz * bary.z;
    float3 localNormal = normalize(v0.normal.xyz * bary.x + v1.normal.xyz * bary.y + v2.normal.xyz * bary.z);
    float4 localTangent = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;
    float2 uv = v0.uv.xy * bary.x + v1.uv.xy * bary.y + v2.uv.xy * bary.z;
    float3 worldPosition = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 worldNormal = normalize(mul(localNormal, (float3x3)WorldToObject3x4()));
    float3 worldTangent = normalize(mul((float3x3)ObjectToWorld3x4(), localTangent.xyz));
    worldTangent = normalize(worldTangent - worldNormal * dot(worldNormal, worldTangent));
    float3 worldBitangent = normalize(cross(worldNormal, worldTangent)) *
        (localTangent.w < 0.0f ? -1.0f : 1.0f);
    float3 tangentNormal = g_Textures[NonUniformResourceIndex(material.texture0.w)]
        .SampleLevel(g_LinearSampler, uv, 0.0f).xyz * 2.0f - 1.0f;
    tangentNormal.xy *= material.factors.x;
    worldNormal = normalize(worldTangent * tangentNormal.x +
        worldBitangent * tangentNormal.y + worldNormal * tangentNormal.z);
    if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE) worldNormal = -worldNormal;

    float4 baseSample = g_Textures[NonUniformResourceIndex(material.texture0.x)]
        .SampleLevel(g_LinearSampler, uv, 0.0f);
    float roughnessSample = g_Textures[NonUniformResourceIndex(material.texture0.y)]
        .SampleLevel(g_LinearSampler, uv, 0.0f).g;
    float metallicSample = g_Textures[NonUniformResourceIndex(material.texture0.z)]
        .SampleLevel(g_LinearSampler, uv, 0.0f).b;
    float3 emissiveSample = g_Textures[NonUniformResourceIndex(material.texture1.x)]
        .SampleLevel(g_LinearSampler, uv, 0.0f).rgb;
    float3 previousWorldPosition = TransformPoint(localPosition, instance.previousLocalToWorld);
    float currentDepth = 0.0f;
    float previousDepth = 0.0f;
    ProjectUV(worldPosition, g_Frame.viewProjection, currentDepth);
    float2 previousUV = ProjectUV(previousWorldPosition, g_Frame.previousViewProjection, previousDepth);

    GpuSurface surface;
    surface.positionDepth = float4(worldPosition, RayTCurrent());
    surface.normalRoughness = float4(worldNormal, clamp(material.factors.z * roughnessSample, 0.045f, 1.0f));
    surface.albedoMetallic = float4(material.baseColor.rgb * baseSample.rgb,
        saturate(material.factors.y * metallicSample));
    surface.emissive = float4(material.emissiveColor.rgb * emissiveSample, 1.0f);
    surface.motion = float4(previousUV, currentDepth, previousDepth);
    surface.ids = uint4(instance.data.x, instanceIndex, geometry.data.w,
        1u | ((instance.data.w & 1u) << 1u));
    uint2 pixel = DispatchRaysIndex().xy;
    g_SurfaceOutput[pixel.y * g_Frame.resolutionFrame.x + pixel.x] = surface;
    payload.value = 1u;
}

float3 DebugColor(
    GpuSurface surface,
    GpuReservoirSample sample,
    GpuReservoirStats stats,
    GpuAcceptance acceptance,
    float3 finalColor)
{
    uint view = g_Frame.modes.y;
    if (view == 0u) return finalColor;
    if (view == 1u) return frac(abs(surface.positionDepth.xyz) * 0.2f);
    if (view == 2u) return normalize(surface.normalRoughness.xyz) * 0.5f + 0.5f;
    if (view == 3u) return surface.albedoMetallic.rgb;
    if (view == 4u) {
        if (sample.sourceType == 1u) return float3(1, 0.2f, 0.1f);
        if (sample.sourceType == 2u) return float3(0.1f, 1, 0.2f);
        if (sample.sourceType == 3u) return float3(0.1f, 0.3f, 1);
        return 0.0f.xxx;
    }
    if (view == 5u) {
        uint value = Hash(sample.stableID);
        return float3(value & 255u, (value >> 8) & 255u, (value >> 16) & 255u) / 255.0f;
    }
    if (view == 6u) return log2(1.0f + stats.selectedPHat).xxx * 0.2f;
    if (view == 7u) return saturate(stats.M / max((float)g_Frame.algorithm.y, 1.0f)).xxx;
    if (view == 8u) return log2(1.0f + stats.W).xxx * 0.2f;
    if (view == 9u) return acceptance.temporalAccepted != 0u ? float3(0, 1, 0) : float3(1, 0, 0);
    if (view == 10u) return acceptance.spatialAccepted != 0u ? float3(0, 1, 0) : float3(1, 0, 0);
    if (view == 11u) {
        float sampleCount = g_Frame.modes.x == 2u
            ? max((float)g_Frame.sampling.y, 1.0f)
            : max((float)g_Frame.sampling.x, 1.0f);
        return saturate(acceptance.visibility / sampleCount).xxx;
    }
    return finalColor;
}

uint TraceVisibility(GpuSurface surface, CandidateEvaluation evaluation)
{
    if ((surface.ids.w & 2u) == 0u || !(Luminance(evaluation.contribution) > 0.0f)) return 1u;
    RayDesc ray;
    ray.Origin = surface.positionDepth.xyz + normalize(surface.normalRoughness.xyz) * g_Frame.reuseThresholds.w;
    ray.Direction = evaluation.direction;
    ray.TMin = g_Frame.reuseThresholds.w;
    ray.TMax = max(evaluation.distance, ray.TMin);
    RayPayload payload = {1u, 1u};
    TraceRay(g_Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 2u, 0u, 0u, 0u, ray, payload);
    return payload.value;
}

[shader("raygeneration")]
void VisibilityRayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint index = pixel.y * g_Frame.resolutionFrame.x + pixel.x;
    GpuSurface surface = g_SurfaceCurrent[index];
    GpuReservoirSample sample = g_ReservoirCurrentSample[index];
    GpuReservoirStats stats = g_ReservoirCurrentStats[index];
    GpuAcceptance laneAcceptance = g_AcceptanceInput[index];
    uint sampleIndex = g_Frame.sampling.z;
    uint sampleCount = max(g_Frame.sampling.x, 1u);
    bool firstSample = sampleIndex == 0u;
    bool lastSample = sampleIndex + 1u >= sampleCount;
    float3 color = firstSample ? surface.emissive.rgb : g_HdrOutput[index].rgb;
    uint visibility = 0u;
    if ((surface.ids.w & 1u) != 0u) {
        if (sample.sourceType != 0u && stats.W > 0.0f) {
            CandidateEvaluation evaluation = EvaluateCandidate(surface, sample);
            visibility = TraceVisibility(surface, evaluation);
            color += evaluation.contribution * stats.W * visibility / sampleCount;
        }
    }
    else if ((surface.ids.w & 2u) != 0u) {
        visibility = 1u;
    }

    GpuAcceptance acceptance = (GpuAcceptance)0;
    if (!firstSample) acceptance = g_AcceptanceOutput[index];
    acceptance.temporalAccepted += laneAcceptance.temporalAccepted;
    acceptance.spatialAccepted += laneAcceptance.spatialAccepted;
    acceptance.spatialRejected += laneAcceptance.spatialRejected;
    acceptance.visibility += visibility;
    g_AcceptanceOutput[index] = acceptance;
    float3 outputColor = lastSample
        ? DebugColor(surface, sample, stats, acceptance, color)
        : color;
    g_HdrOutput[index] = float4(outputColor, 1.0f);
}

[shader("raygeneration")]
void ReferenceRayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint index = pixel.y * g_Frame.resolutionFrame.x + pixel.x;
    GpuSurface surface = g_SurfaceCurrent[index];
    float3 color = surface.emissive.rgb;
    uint visibleCount = 0u;
    if ((surface.ids.w & 1u) != 0u) {
        uint randomState = Hash(index ^ g_Frame.resolutionFrame.w ^
            Hash(g_Frame.resolutionFrame.z + 0x51ed270bu));
        float3 direct = 0.0f.xxx;
        uint candidateCount = max(g_Frame.sampling.y, 1u);
        [loop] for (uint candidateIndex = 0u; candidateIndex < candidateCount; ++candidateIndex) {
            GpuReservoirSample sample = GenerateCandidate(randomState);
            CandidateEvaluation evaluation = EvaluateCandidate(surface, sample);
            if (!(evaluation.proposalPdf > RESTIR_EPSILON)) continue;
            uint visibility = TraceVisibility(surface, evaluation);
            visibleCount += visibility;
            direct += evaluation.contribution * visibility / evaluation.proposalPdf;
        }
        color += direct / candidateCount;
    }
    GpuAcceptance acceptance = (GpuAcceptance)0;
    acceptance.visibility = visibleCount;
    g_AcceptanceOutput[index] = acceptance;
    GpuReservoirSample emptySample = (GpuReservoirSample)0;
    GpuReservoirStats emptyStats = (GpuReservoirStats)0;
    g_HdrOutput[index] = float4(DebugColor(surface, emptySample, emptyStats, acceptance, color), 1.0f);
}

#ifndef RESTIR_DI_COMMON_HLSLI
#define RESTIR_DI_COMMON_HLSLI

static const float RESTIR_PI = 3.14159265358979323846f;
static const float RESTIR_EPSILON = 1e-6f;
static const uint RESTIR_INVALID_INDEX = 0xFFFFFFFFu;
static const uint RESTIR_MAX_MATERIAL_TEXTURES = 256u;

// 这些 enum 是 GPU 端协议的一部分，数值必须分别与
// RestirDIShared.h / RestirDISettings.h 中的 C++ enum 对齐。
// 只有具有封闭语义集合的标志和模式使用 enum；寄存器、线程组尺寸及数学常数
// 仍保持原样，避免把资源布局伪装成算法状态。
enum RestirSourceType
{
    InvalidSource = 0,
    AnalyticSource = 1,
    EmissiveTriangleSource = 2,
    EnvironmentSource = 3
};

enum RestirRenderMode
{
    UnbiasedRestirMode = 0,
    ReferenceMode = 1
};

enum RestirDebugView
{
    FinalDebugView = 0,
    SurfaceDebugView = 1,
    NormalDebugView = 2,
    AlbedoDebugView = 3,
    SourceTypeDebugView = 4,
    SourceIDDebugView = 5,
    PHatDebugView = 6,
    ReservoirMDebugView = 7,
    SupportRatioDebugView = 8,
    ReservoirWDebugView = 9,
    TemporalAcceptanceDebugView = 10,
    SpatialAcceptanceDebugView = 11,
    VisibilityDebugView = 12
};

// LightType 的数值来自引擎 Light 枚举；这里保留同一顺序，便于阅读解析灯的
// 距离衰减和 Spot 锥角分支。
enum RestirAnalyticLightType
{
    DirectionalLight = 0,
    PointLight = 1,
    SpotLight = 2
};

enum RestirInstanceMask
{
    PrimaryInstanceMask = 1,
    ShadowInstanceMask = 2
};

enum RestirInstanceFlags
{
    ReceivesShadowInstance = 1
};

enum RestirMaterialFlags
{
    AlphaTestMaterial = 1,
    DoubleSidedMaterial = 2
};

enum RestirSurfaceFlags
{
    ValidSurface = 1,
    ReceivesShadowSurface = 2,
    // 非命中表面没有 ValidSurface；复用同一位标记背景，使 VisibilityRayGen
    // 能在不增加 Surface 字段的情况下把环境颜色保留下来。
    BackgroundSurface = 2
};

enum RestirRayType
{
    PrimaryRay = 0,
    VisibilityRay = 1
};

// HLSL 有 float4x4；这里显式存四行是跨 C++/HLSL ABI 选择，不是功能限制。
// 它固定 StructuredBuffer 布局，并让 MulRow 明确表达 DSMEngine 的行向量语义，
// 避免默认 column_major 或 mul 参数顺序引入不可见的转置。
struct GpuMatrix { float4 row0; float4 row1; float4 row2; float4 row3; };
struct GpuVertex { float4 position; float4 normal; float4 tangent; float4 uv; };
struct GpuGeometry { uint4 data; };
struct GpuInstance { GpuMatrix currentLocalToWorld; GpuMatrix previousLocalToWorld; uint4 data; };
struct GpuMaterial
{
    float4 baseColor;
    float4 emissiveColor;
    float4 factors;
    uint4 texture0;
    uint4 texture1;
};
struct GpuAnalyticLight
{
    float4 color;
    float4 positionInvRange;
    float4 directionType;
    float4 anglesPower;
    uint4 metadata;
};
struct GpuAliasEntry { float probability; uint alias; float pmf; uint padding; };
struct GpuEmissiveTriangle { uint4 data; float4 areaPower; };
struct GpuSurface
{
    float4 positionDepth;
    float4 normalRoughness;
    float4 albedoMetallic;
    float4 emissive;
    float4 motion;
    uint4 ids;
};
struct GpuReservoirSample { uint sourceType; uint stableID; uint itemIndex; uint sampleSeed; };
struct GpuReservoirStats { float weightSum; float M; float W; float normalizationM; };
struct GpuAcceptance { uint temporalAccepted; uint spatialAccepted; uint spatialRejected; uint visibility; };
struct GpuFrameConstants
{
    GpuMatrix viewProjection;
    GpuMatrix inverseViewProjection;
    GpuMatrix previousViewProjection;
    float4 cameraExposure;
    float4 reuseThresholds;
    float4 domainProbabilities;
    float4 rayEnvironment;
    uint4 resolutionFrame;
    uint4 sourceCounts;
    uint4 algorithm;
    uint4 modes;
    uint4 environmentInfo;
};

ConstantBuffer<GpuFrameConstants> g_Frame : register(b0);
StructuredBuffer<GpuVertex> g_Vertices : register(t1);
StructuredBuffer<uint> g_Indices : register(t2);
StructuredBuffer<GpuGeometry> g_Geometries : register(t3);
StructuredBuffer<GpuInstance> g_Instances : register(t4);
StructuredBuffer<GpuMaterial> g_Materials : register(t5);
StructuredBuffer<GpuAnalyticLight> g_Lights : register(t6);
StructuredBuffer<GpuAliasEntry> g_LightAlias : register(t7);
StructuredBuffer<GpuEmissiveTriangle> g_EmissiveTriangles : register(t8);
StructuredBuffer<GpuAliasEntry> g_EmissiveAlias : register(t9);
StructuredBuffer<float4> g_EnvironmentPixels : register(t10);
StructuredBuffer<GpuAliasEntry> g_EnvironmentAlias : register(t11);
StructuredBuffer<GpuSurface> g_SurfaceCurrent : register(t12);
StructuredBuffer<GpuSurface> g_SurfacePrevious : register(t13);
StructuredBuffer<GpuReservoirSample> g_ReservoirCurrentSample : register(t14);
StructuredBuffer<GpuReservoirStats> g_ReservoirCurrentStats : register(t15);
StructuredBuffer<GpuReservoirSample> g_ReservoirHistorySample : register(t16);
StructuredBuffer<GpuReservoirStats> g_ReservoirHistoryStats : register(t17);
StructuredBuffer<float4> g_HdrInput : register(t18);
StructuredBuffer<GpuAcceptance> g_AcceptanceInput : register(t19);

RWStructuredBuffer<GpuSurface> g_SurfaceOutput : register(u0);
RWStructuredBuffer<GpuReservoirSample> g_ReservoirSampleOutput : register(u1);
RWStructuredBuffer<GpuReservoirStats> g_ReservoirStatsOutput : register(u2);
RWStructuredBuffer<GpuAcceptance> g_AcceptanceOutput : register(u3);
RWStructuredBuffer<float4> g_HdrOutput : register(u4);
RWStructuredBuffer<uint4> g_ValidationCounters : register(u5);

SamplerState g_LinearSampler : register(s0);
Texture2D<float4> g_Textures[RESTIR_MAX_MATERIAL_TEXTURES] : register(t0, space1);

float4 MulRow(float4 value, GpuMatrix matrix)
{
    return float4(
        dot(value, float4(matrix.row0.x, matrix.row1.x, matrix.row2.x, matrix.row3.x)),
        dot(value, float4(matrix.row0.y, matrix.row1.y, matrix.row2.y, matrix.row3.y)),
        dot(value, float4(matrix.row0.z, matrix.row1.z, matrix.row2.z, matrix.row3.z)),
        dot(value, float4(matrix.row0.w, matrix.row1.w, matrix.row2.w, matrix.row3.w)));
}

float3 TransformPoint(float3 value, GpuMatrix matrix)
{
    float4 transformed = MulRow(float4(value, 1.0f), matrix);
    return transformed.xyz / max(abs(transformed.w), RESTIR_EPSILON);
}

float3 TransformDirection(float3 value, GpuMatrix matrix)
{
    return normalize(MulRow(float4(value, 0.0f), matrix).xyz);
}

uint Hash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float RandomFloat(inout uint state)
{
    state = Hash(state + 0x9e3779b9u);
    return (state >> 8) * (1.0f / 16777216.0f);
}

float Luminance(float3 color)
{
    return max(dot(color, float3(0.2126f, 0.7152f, 0.0722f)), 0.0f);
}

float3 SafeNormalize(float3 value, float3 fallback)
{
    float squaredLength = dot(value, value);
    return squaredLength > RESTIR_EPSILON * RESTIR_EPSILON
        ? value * rsqrt(squaredLength) : fallback;
}

float3 EvaluateBRDF(GpuSurface surface, float3 lightDirection)
{
    float3 normal = SafeNormalize(surface.normalRoughness.xyz, float3(0, 1, 0));
    float3 viewDirection = SafeNormalize(
        g_Frame.cameraExposure.xyz - surface.positionDepth.xyz, normal);
    lightDirection = SafeNormalize(lightDirection, normal);
    float3 halfDirection = SafeNormalize(lightDirection + viewDirection, normal);
    float NoV = max(dot(normal, viewDirection), 1e-4f);
    float NoL = saturate(dot(normal, lightDirection));
    float NoH = saturate(dot(normal, halfDirection));
    float VoH = saturate(dot(viewDirection, halfDirection));
    float roughness = clamp(surface.normalRoughness.w, 0.045f, 1.0f);
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denominator = NoH * NoH * (alpha2 - 1.0f) + 1.0f;
    float D = alpha2 / max(RESTIR_PI * denominator * denominator, RESTIR_EPSILON);
    float k = (roughness + 1.0f);
    k = k * k * 0.125f;
    float Gv = NoV / lerp(NoV, 1.0f, k);
    float Gl = NoL / lerp(NoL, 1.0f, k);
    float3 f0 = lerp(0.04f.xxx, surface.albedoMetallic.rgb, surface.albedoMetallic.w);
    float3 F = f0 + (1.0f - f0) * pow(1.0f - VoH, 5.0f);
    float3 specular = D * Gv * Gl * F / max(4.0f * NoV * NoL, RESTIR_EPSILON);
    float3 diffuse = surface.albedoMetallic.rgb * (1.0f - surface.albedoMetallic.w) / RESTIR_PI;
    return (diffuse + specular) * NoL;
}

uint ResolveAliasLight(float randomValue, out float pmf)
{
    uint count = g_Frame.sourceCounts.x;
    float scaled = saturate(randomValue) * count;
    uint column = min((uint)scaled, count - 1u);
    GpuAliasEntry entry = g_LightAlias[column];
    uint selected = frac(scaled) < entry.probability ? column : entry.alias;
    pmf = g_LightAlias[selected].pmf;
    return selected;
}

uint ResolveAliasEmissive(float randomValue, out float pmf)
{
    uint count = g_Frame.sourceCounts.y;
    float scaled = saturate(randomValue) * count;
    uint column = min((uint)scaled, count - 1u);
    GpuAliasEntry entry = g_EmissiveAlias[column];
    uint selected = frac(scaled) < entry.probability ? column : entry.alias;
    pmf = g_EmissiveAlias[selected].pmf;
    return selected;
}

uint ResolveAliasEnvironment(float randomValue, out float pmf)
{
    uint count = g_Frame.sourceCounts.z;
    float scaled = saturate(randomValue) * count;
    uint column = min((uint)scaled, count - 1u);
    GpuAliasEntry entry = g_EnvironmentAlias[column];
    uint selected = frac(scaled) < entry.probability ? column : entry.alias;
    pmf = g_EnvironmentAlias[selected].pmf;
    return selected;
}

float3 EnvironmentDirection(uint itemIndex, uint seed, out float solidAngle)
{
    uint width = max(g_Frame.environmentInfo.x, 1u);
    uint height = max(g_Frame.environmentInfo.y, 1u);
    uint x = itemIndex % width;
    uint y = min(itemIndex / width, height - 1u);
    uint randomState = seed;
    float2 jitter = float2(RandomFloat(randomState), RandomFloat(randomState));
    float theta0 = RESTIR_PI * y / height;
    float theta1 = RESTIR_PI * (y + 1u) / height;
    float cosTheta = lerp(cos(theta0), cos(theta1), jitter.y);
    float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));
    float phi = 2.0f * RESTIR_PI * ((x + jitter.x) / width - 0.5f) + g_Frame.rayEnvironment.x;
    solidAngle = (2.0f * RESTIR_PI / width) * (cos(theta0) - cos(theta1));
    return float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));
}

float3 SampleEnvironment(float3 direction)
{
    uint width = max(g_Frame.environmentInfo.x, 1u);
    uint height = max(g_Frame.environmentInfo.y, 1u);
    float phi = atan2(direction.z, direction.x) - g_Frame.rayEnvironment.x;
    float u = frac(phi / (2.0f * RESTIR_PI) + 0.5f);
    float v = acos(clamp(direction.y, -1.0f, 1.0f)) / RESTIR_PI;
    uint x = min((uint)(u * width), width - 1u);
    uint y = min((uint)(v * height), height - 1u);
    return g_EnvironmentPixels[y * width + x].rgb * g_Frame.domainProbabilities.w;
}

// 论文第 5 节的候选生成：先按域功率选择一个 proposal domain，再在域内
// 用 Walker Alias Table 选择离散对象。sampleSeed 保留域内连续采样的随机状态，
// 使跨帧重投影后可以重新评价同一个稳定候选，而无需把三角形位置存进 Reservoir。
GpuReservoirSample GenerateCandidate(inout uint randomState)
{
    GpuReservoirSample sample = {InvalidSource, RESTIR_INVALID_INDEX, RESTIR_INVALID_INDEX, randomState};
    float selector = RandomFloat(randomState);
    float analyticEnd = g_Frame.domainProbabilities.x;
    float emissiveEnd = analyticEnd + g_Frame.domainProbabilities.y;
    float pmf = 0.0f;
    if (selector < analyticEnd && g_Frame.sourceCounts.x > 0u) {
        sample.sourceType = AnalyticSource;
        sample.itemIndex = ResolveAliasLight(RandomFloat(randomState), pmf);
        sample.stableID = g_Lights[sample.itemIndex].metadata.x;
    }
    else if (selector < emissiveEnd && g_Frame.sourceCounts.y > 0u) {
        sample.sourceType = EmissiveTriangleSource;
        sample.itemIndex = ResolveAliasEmissive(RandomFloat(randomState), pmf);
        sample.stableID = g_EmissiveTriangles[sample.itemIndex].data.w;
    }
    else if (g_Frame.sourceCounts.z > 0u && g_Frame.domainProbabilities.z > 0.0f) {
        sample.sourceType = EnvironmentSource;
        sample.itemIndex = ResolveAliasEnvironment(RandomFloat(randomState), pmf);
        sample.stableID = 0xE0000000u ^ sample.itemIndex;
    }
    sample.sampleSeed = Hash(randomState);
    return sample;
}

struct CandidateEvaluation
{
    float3 contribution;
    float3 direction;
    float distance;
    float proposalPdf;
};

bool HasValidProposalPdf(CandidateEvaluation evaluation)
{
    return evaluation.proposalPdf > 0.0f && isfinite(evaluation.proposalPdf);
}

// 论文第 3.1 节的 target function 使用未遮挡直接光 pHat = luminance(rho Le G)。
// 这里先计算 contribution 和 proposal PDF q，故初始 RIS 的权重就是 pHat / q；
// 可见性留到最终 VisibilityRayGen 才追踪，这对应论文推荐的“只对幸存样本发阴影光线”。
CandidateEvaluation EvaluateCandidate(GpuSurface surface, GpuReservoirSample sample)
{
    CandidateEvaluation result = (CandidateEvaluation)0;
    result.distance = g_Frame.rayEnvironment.y;
    if ((surface.ids.w & ValidSurface) == 0u) return result;

    if (sample.sourceType == AnalyticSource && sample.itemIndex < g_Frame.sourceCounts.x) {
        GpuAnalyticLight light = g_Lights[sample.itemIndex];
        if (sample.stableID != light.metadata.x) return result;
        float3 lightDirection = SafeNormalize(light.directionType.xyz, surface.normalRoughness.xyz);
        float attenuation = 1.0f;
        if ((uint)light.directionType.w != DirectionalLight) {
            float3 toLight = light.positionInvRange.xyz - surface.positionDepth.xyz;
            float distanceSquared = max(dot(toLight, toLight), 1e-4f);
            float distanceToLight = sqrt(distanceSquared);
            lightDirection = SafeNormalize(toLight, surface.normalRoughness.xyz);
            float rangeFactor = distanceSquared * light.positionInvRange.w * light.positionInvRange.w;
            float smoothFactor = max(1.0f - rangeFactor * rangeFactor, 0.0f);
            attenuation = smoothFactor * smoothFactor / distanceSquared;
            result.distance = max(distanceToLight - g_Frame.reuseThresholds.w, g_Frame.reuseThresholds.w);
            if ((uint)light.directionType.w == SpotLight) {
                float cosOuter = cos(light.anglesPower.y);
                float scale = 1.0f / max(cos(light.anglesPower.x) - cosOuter, 1e-4f);
                float spot = saturate(dot(light.directionType.xyz, lightDirection) * scale - cosOuter * scale);
                attenuation *= spot * spot;
            }
        }
        result.direction = lightDirection;
        result.contribution = EvaluateBRDF(surface, lightDirection) * light.color.rgb * attenuation;
        result.proposalPdf = g_Frame.domainProbabilities.x * g_LightAlias[sample.itemIndex].pmf;
    }
    else if (sample.sourceType == EmissiveTriangleSource && sample.itemIndex < g_Frame.sourceCounts.y) {
        GpuEmissiveTriangle emissiveTriangle = g_EmissiveTriangles[sample.itemIndex];
        if (sample.stableID != emissiveTriangle.data.w) return result;
        GpuInstance instance = g_Instances[emissiveTriangle.data.x];
        uint i0 = g_Indices[emissiveTriangle.data.y];
        uint i1 = g_Indices[emissiveTriangle.data.y + 1u];
        uint i2 = g_Indices[emissiveTriangle.data.y + 2u];
        uint geometryBase = instance.data.y;
        uint vertexBase = 0u;
        [loop] for (uint geometryIndex = 0u; geometryIndex < instance.data.z; ++geometryIndex) {
            GpuGeometry geometry = g_Geometries[geometryBase + geometryIndex];
            if (emissiveTriangle.data.y >= geometry.data.y &&
                emissiveTriangle.data.y < geometry.data.y + geometry.data.z) {
                vertexBase = geometry.data.x;
                break;
            }
        }
        GpuVertex v0 = g_Vertices[vertexBase + i0];
        GpuVertex v1 = g_Vertices[vertexBase + i1];
        GpuVertex v2 = g_Vertices[vertexBase + i2];
        uint randomState = sample.sampleSeed;
        float root = sqrt(RandomFloat(randomState));
        float b0 = 1.0f - root;
        float b1 = root * (1.0f - RandomFloat(randomState));
        float b2 = 1.0f - b0 - b1;
        float3 p0 = TransformPoint(v0.position.xyz, instance.currentLocalToWorld);
        float3 p1 = TransformPoint(v1.position.xyz, instance.currentLocalToWorld);
        float3 p2 = TransformPoint(v2.position.xyz, instance.currentLocalToWorld);
        float3 lightPosition = p0 * b0 + p1 * b1 + p2 * b2;
        float3 toLight = lightPosition - surface.positionDepth.xyz;
        float distanceSquared = max(dot(toLight, toLight), 1e-6f);
        float distanceToLight = sqrt(distanceSquared);
        float3 lightDirection = SafeNormalize(toLight, surface.normalRoughness.xyz);
        float3 lightNormal = SafeNormalize(cross(p1 - p0, p2 - p0), -lightDirection);
        GpuMaterial material = g_Materials[emissiveTriangle.data.z];
        float cosineAtLight = dot(lightNormal, -lightDirection);
        if ((material.texture1.z & DoubleSidedMaterial) != 0u) cosineAtLight = abs(cosineAtLight);
        else cosineAtLight = saturate(cosineAtLight);
        float2 uv = v0.uv.xy * b0 + v1.uv.xy * b1 + v2.uv.xy * b2;
        float3 emission = material.emissiveColor.rgb *
            g_Textures[NonUniformResourceIndex(material.texture1.x)].SampleLevel(g_LinearSampler, uv, 0.0f).rgb;
        result.direction = lightDirection;
        result.distance = max(distanceToLight - g_Frame.reuseThresholds.w, g_Frame.reuseThresholds.w);
        result.contribution = EvaluateBRDF(surface, lightDirection) * emission * cosineAtLight / distanceSquared;
        const float triangleArea = emissiveTriangle.areaPower.x;
        result.proposalPdf = triangleArea > 0.0f && isfinite(triangleArea)
            ? g_Frame.domainProbabilities.y * g_EmissiveAlias[sample.itemIndex].pmf /
                triangleArea
            : 0.0f;
    }
    else if (sample.sourceType == EnvironmentSource && sample.itemIndex < g_Frame.sourceCounts.z) {
        if (sample.stableID != (0xE0000000u ^ sample.itemIndex)) return result;
        float solidAngle = 0.0f;
        result.direction = EnvironmentDirection(sample.itemIndex, sample.sampleSeed, solidAngle);
        result.contribution = EvaluateBRDF(surface, result.direction) *
            g_EnvironmentPixels[sample.itemIndex].rgb * g_Frame.domainProbabilities.w;
        result.proposalPdf = solidAngle > 0.0f && isfinite(solidAngle)
            ? g_Frame.domainProbabilities.z * g_EnvironmentAlias[sample.itemIndex].pmf /
                solidAngle
            : 0.0f;
    }
    if (!all(isfinite(result.contribution)) || !all(isfinite(result.direction)) ||
        !isfinite(result.distance)) {
        result.contribution = 0.0f.xxx;
        result.direction = 0.0f.xxx;
        result.distance = 0.0f;
        result.proposalPdf = 0.0f;
    }
    return result;
}

void ReservoirClear(out GpuReservoirSample sample, out GpuReservoirStats stats)
{
    sample.sourceType = InvalidSource;
    sample.stableID = RESTIR_INVALID_INDEX;
    sample.itemIndex = RESTIR_INVALID_INDEX;
    sample.sampleSeed = 0u;
    stats.weightSum = 0.0f;
    stats.M = 0.0f;
    stats.W = 0.0f;
    stats.normalizationM = 0.0f;
}

void ReservoirInvalidateSample(
    inout GpuReservoirSample sample,
    inout GpuReservoirStats stats)
{
    // 空代表样本不等于空输入流。保留 M，后续 Algorithm 6 在判断最终样本
    // 是否位于本来源支持域内时仍需计入这批候选。
    sample.sourceType = InvalidSource;
    sample.stableID = RESTIR_INVALID_INDEX;
    sample.itemIndex = RESTIR_INVALID_INDEX;
    sample.sampleSeed = 0u;
    stats.weightSum = 0.0f;
    stats.W = 0.0f;
    stats.normalizationM = 0.0f;
}

float EvaluateTargetPHat(GpuSurface surface, GpuReservoirSample sample)
{
    CandidateEvaluation evaluation = EvaluateCandidate(surface, sample);
    // Algorithm 6 的支持判定必须同时满足 target 与 proposal 支持。
    // 例如退化三角形/零立体角候选可能暂时算出非零贡献，但 q=0 时
    // 不能参与 merge，也不能计入 Z。
    if (!HasValidProposalPdf(evaluation)) return 0.0f;
    float pHat = Luminance(evaluation.contribution);
    return pHat > 0.0f && isfinite(pHat) ? pHat : 0.0f;
}

void ReservoirUpdate(
    inout GpuReservoirSample reservoirSample,
    inout GpuReservoirStats reservoirStats,
    GpuReservoirSample candidate,
    float candidateWeight,
    float candidateM,
    inout uint randomState)
{
    // 论文 Algorithm 2：以 w_i / sum(w) 的概率替换当前样本，同时累计 M 和
    // weightSum。candidateM 在时空合并时可以代表一个上游 Reservoir 的样本数。
    const float acceptedM = isfinite(candidateM) ? max(candidateM, 0.0f) : 0.0f;
    reservoirStats.M += acceptedM;
    if (!isfinite(reservoirStats.M)) {
        ReservoirClear(reservoirSample, reservoirStats);
        return;
    }
    if (!(candidateWeight > 0.0f) || !isfinite(candidateWeight) ||
        candidate.sourceType == InvalidSource) return;
    reservoirStats.weightSum += candidateWeight;
    if (!isfinite(reservoirStats.weightSum) || !(reservoirStats.weightSum > 0.0f)) {
        ReservoirClear(reservoirSample, reservoirStats);
        return;
    }
    if (RandomFloat(randomState) * reservoirStats.weightSum < candidateWeight) {
        reservoirSample = candidate;
    }
}

void ReservoirMergeSource(
    GpuSurface destinationSurface,
    GpuReservoirSample sourceSample,
    GpuReservoirStats sourceStats,
    float sourceM,
    inout GpuReservoirSample outputSample,
    inout GpuReservoirStats outputStats,
    inout uint randomState)
{
    const float acceptedM = isfinite(sourceM) ? max(sourceM, 0.0f) : 0.0f;
    const float destinationPHat = EvaluateTargetPHat(destinationSurface, sourceSample);
    const float mergeWeight = destinationPHat * sourceStats.W * acceptedM;
    // 论文 Algorithm 6 第 4 行。每个来源 Reservoir 必须作为原子输入重新
    // 合并；无偏 W 下不能直接复制来源的 raw weightSum。
    ReservoirUpdate(outputSample, outputStats, sourceSample,
        mergeWeight, acceptedM, randomState);
}

float ReservoirSupportM(
    GpuSurface originSurface,
    GpuReservoirSample selectedSample,
    float sourceM)
{
    if (!(sourceM > 0.0f) || !isfinite(sourceM)) return 0.0f;
    // Algorithm 6 第 7–9 行：uniform MIS 只计入其 target 支持最终样本的来源。
    return EvaluateTargetPHat(originSurface, selectedSample) > 0.0f ? sourceM : 0.0f;
}

void ReservoirFinalize(
    GpuSurface destinationSurface,
    inout GpuReservoirSample sample,
    inout GpuReservoirStats stats,
    float normalizationM)
{
    stats.normalizationM = isfinite(normalizationM) ? max(normalizationM, 0.0f) : 0.0f;
    const float destinationPHat = EvaluateTargetPHat(destinationSurface, sample);
    if (!(stats.weightSum > 0.0f) || !(stats.M > 0.0f) ||
        !(stats.normalizationM > 0.0f) || !(destinationPHat > 0.0f) ||
        !isfinite(stats.weightSum) || !isfinite(stats.M)) {
        ReservoirInvalidateSample(sample, stats);
        return;
    }
    // 初始 RIS 对应 Eq. (6)，其 Z=M；复用对应 Eq. (20)/Algorithm 6，
    // Z 只包含支持最终样本的输入流，消除不同半球支持域造成的暗偏。
    stats.W = stats.weightSum / (stats.normalizationM * destinationPHat);
    if (!isfinite(stats.W) || !(stats.W > 0.0f)) ReservoirInvalidateSample(sample, stats);
}

bool TemporalSurfaceCompatible(GpuSurface current, GpuSurface history)
{
    // 这里只验证 motion reprojection 是否仍指向同一表面。无偏支持域修正由
    // Algorithm 6 的 Z 完成，不能用该测试代替。
    if ((current.ids.w & ValidSurface) == 0u || (history.ids.w & ValidSurface) == 0u) return false;
    if (current.ids.x != history.ids.x || current.ids.z != history.ids.z) return false;
    float normalSimilarity = dot(
        SafeNormalize(current.normalRoughness.xyz, float3(0, 1, 0)),
        SafeNormalize(history.normalRoughness.xyz, float3(0, 1, 0)));
    float relativeDepth = abs(history.motion.z - current.motion.w) /
        max(abs(current.motion.w), 1e-3f);
    return normalSimilarity >= g_Frame.reuseThresholds.x &&
        relativeDepth <= g_Frame.reuseThresholds.y;
}

#endif

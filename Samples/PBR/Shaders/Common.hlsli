#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

// 默认采样器
SamplerState gAnisoWrapSampler : register(s0);
// 比较采样器，用于阴影贴图采样
SamplerComparisonState gShadowSampler : register(s1);


float2 EncodeNormal(float3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    normal.xy = (normal.z >= 0) ? normal.xy : (1.0 - abs(normal.yx)) * sign(normal.xy);
    return normal.xy * 0.5f + 0.5f;
}

float3 DecodeNormal(float2 val)
{
    val = val * 2.0 - 1.0;
    float3 n = float3(val.xy, 1.0 - abs(val.x) - abs(val.y));
    float t = max(-n.z, 0.0);
    n.xy = (n.z >= 0) ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);;
    return normalize(n);
}

float DistanceSquared(float3 p1, float3 p2)
{
    return dot(p1 - p2, p1 - p2);
}

float GetLinearDepth(float depth, float4x4 projMatrix)
{
    return projMatrix[3][2] / (depth - projMatrix[2][2]);
}

#endif // __COMMON_HLSLI__
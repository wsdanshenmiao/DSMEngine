#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

// 默认采样器
SamplerState gDefaultSampler : register(s0);
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
    n.xy -= t * sign(n.xy);
    return normalize(n);
}

#endif // __COMMON_HLSLI__
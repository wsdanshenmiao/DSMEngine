#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

// 常用的采样器
SamplerState gPointWrapSampler : register(s0);
SamplerState gLinearWrapSampler : register(s1);
SamplerState gAnisoWrapSampler : register(s2);
SamplerState gPointClampSampler : register(s3);
SamplerState gLinearClampSampler : register(s4);
SamplerState gAnisoClampSampler : register(s5);
SamplerState gPointBorderSampler : register(s6);
SamplerState gLinearBorderSampler : register(s7);
// 比较采样器，用于阴影贴图采样
SamplerComparisonState gShadowSampler : register(s8);


float2 EncodeFloat3ToFloat2(float3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    normal.xy = (normal.z >= 0) ? normal.xy : (1.0 - abs(normal.yx)) * sign(normal.xy);
    return normal.xy * 0.5f + 0.5f;
}

float3 DecodeFloat2ToFloat3(float2 val)
{
    val = val * 2.0 - 1.0;
    float3 n = float3(val.xy, 1.0 - abs(val.x) - abs(val.y));
    float t = max(-n.z, 0.0);
    n.xy = (n.z >= 0) ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);;
    return normalize(n);
}


uint EncodeFloat4ToUint(float4 color)
{
    uint4 packed = uint4(round(saturate(color) * 255.0f));
    return (packed.x) | (packed.y << 8) | (packed.z << 16) | (packed.w << 24);
}

float4 DecodeUintToFloat4(uint packed)
{
    float4 color;
    color.x = (packed & 0x000000FF) / 255.0f;
    color.y = ((packed & 0x0000FF00) >> 8) / 255.0f;
    color.z = ((packed & 0x00FF0000) >> 16) / 255.0f;
    color.w = ((packed & 0xFF000000) >> 24) / 255.0f;
    return color;
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
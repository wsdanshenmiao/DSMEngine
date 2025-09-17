#ifndef __SHADOW_HLSLI__
#define __SHADOW_HLSLI__

#include "Common.hlsli"
#include "ResourceData.h"
#include "Surface.hlsli"

struct DirectionalShadowData
{
    int tileIndex;
};

Texture2D<float> gShadowMap : register(t7);

ConstantBuffer<ShadowConstants> gShadowConstants : register(b4);

float SampleDirectionalShadow(float3 posSS)
{
    float depth = posSS.z;
    return gShadowMap.SampleCmpLevelZero(gShadowSampler, posSS.xy, depth);
}

float GetDirectionalShadowAttenuation(DirectionalShadowData directional, Surface surface)
{
    float4x4 viewProj = gShadowConstants.shadowViewProjs[directional.tileIndex];
    // 变换到 NDC 空间
    float4 posSS = mul(float4(surface.position, 1), viewProj);
    posSS /= posSS.w;
    float2 uv = posSS.xy * float2(0.5, 0.5) + 0.5;
    // 变换到纹理空间
    float3 posTS = float3(uv, posSS.z);

    return SampleDirectionalShadow(posTS);
}


#endif // __SHADOW_HLSLI__
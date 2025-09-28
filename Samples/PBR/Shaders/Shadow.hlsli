#ifndef __SHADOW_HLSLI__
#define __SHADOW_HLSLI__

#include "Common.hlsli"
#include "../ResourceData.h"
#include "Surface.hlsli"

#if defined(DIRECTIONAL_PCF3)
    #define DIRECTIONAL_FILTER_SAMPLES 3
#elif defined(DIRECTIONAL_PCF5)
    #define DIRECTIONAL_FILTER_SAMPLES 5
#elif defined(DIRECTIONAL_PCF7)
    #define DIRECTIONAL_FILTER_SAMPLES 7
#else
    #define DIRECTIONAL_FILTER_SAMPLES 1
#endif

struct DirectionalShadowData
{
    int tileIndex;
};

Texture2D<float> gShadowMap : register(t7);

ConstantBuffer<ShadowConstants> gShadowConstants : register(b4);

float SampleDirectionalShadow(float3 posSS)
{
    int sampleRadius = DIRECTIONAL_FILTER_SAMPLES;
    int area = sampleRadius * sampleRadius;
    int halfRadius = sampleRadius / 2;
    float visibility = 0;
    for(int i = 0; i < area; ++i) {
        int2 offset = int2(i % sampleRadius - halfRadius, i / sampleRadius - halfRadius);
        float shadow = gShadowMap.SampleCmpLevelZero(gShadowSampler, posSS.xy, posSS.z, offset);
        visibility += shadow;
    }
    return visibility / area;
}

float GetDirectionalShadowAttenuation(DirectionalShadowData directional, Surface surface)
{
    float4x4 viewProj = gShadowConstants.shadowViewProjs[directional.tileIndex];
    // 变换到 NDC 空间
    float4 posTS = mul(float4(surface.position, 1), viewProj);

    return SampleDirectionalShadow(posTS.xyz);
}


#endif // __SHADOW_HLSLI__
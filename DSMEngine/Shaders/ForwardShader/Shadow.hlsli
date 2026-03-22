#ifndef __SHADOW_HLSLI__
#define __SHADOW_HLSLI__

#include "Common.hlsli"
#include "ResourceData.h"
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

struct ShadowData
{
    float strength;
    uint cascadeIndex;
};

ConstantBuffer<ShaderResource::ShadowConstants> gShadowConstants : register(b3);
Texture2D<float> gShadowMap : register(t5);

float FadedShadowStrength (float dist, float scale, float fade)
{
    return saturate((1 - dist * scale) * fade);
}

ShadowData GetShadowData(Surface surface)
{
    float recMaxDistance = gShadowConstants.recMaxDistance;
    ShadowData data;
    // 阴影边界处的过渡
    data.strength = FadedShadowStrength(surface.depth, recMaxDistance, gShadowConstants.recDistanceFade);
    
    // 根据表面到视锥体的距离选择级联
    uint cascadeIndex = 0;
    uint cascadeCount = gShadowConstants.cascadeCount;
    if(cascadeCount > 1){
        // 向量比较与点乘避免循环
        float4 cmpVec = (float4)surface.depth > gShadowConstants.cascadeFarPlaneDist;
        float4 cascadeCountVec = float4(cascadeCount > 0, cascadeCount > 1, cascadeCount > 2, cascadeCount > 3);
        float index = dot(cmpVec, cascadeCountVec);
        cascadeIndex = min(uint(index), cascadeCount - 1);
        data.strength *= cascadeIndex == (cascadeCount - 1) ? 
            FadedShadowStrength(surface.depth, recMaxDistance, gShadowConstants.recDistanceFade) : 1;
    }

    data.cascadeIndex = cascadeIndex;
    
    return data;
}

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
    ShadowData shadowData = GetShadowData(surface);
    if(shadowData.strength <= 0)
        return 1.0;

    uint matrixIndex = directional.tileIndex + shadowData.cascadeIndex;
    float4x4 viewProj = gShadowConstants.shadowViewProjs[matrixIndex];
    // 变换到 NDC 空间
    float4 posTS = mul(float4(surface.position, 1), viewProj);

    float shadow = SampleDirectionalShadow(posTS.xyz);
    return lerp(1.0, shadow, shadowData.strength);
}


#endif // __SHADOW_HLSLI__
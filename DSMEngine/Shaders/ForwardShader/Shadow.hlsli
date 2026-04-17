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
    uint cascadeIndex;
    float strength;
    float cascadeBlend;
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
    float recDistanceFade = gShadowConstants.recDistanceFade;
    float cascadeFade = gShadowConstants.cascadeFade;
    ShadowData data;
    // 阴影边界处的过渡
    data.strength = FadedShadowStrength(surface.depth, recMaxDistance, recDistanceFade);
    data.cascadeBlend = 1;
    
    // 根据表面到视锥体的距离选择级联
    uint cascadeIndex = 0;
    uint cascadeCount = gShadowConstants.cascadeCount;
    if(cascadeCount > 1){
        float4 farPlaneDist = gShadowConstants.cascadeFarPlaneDist;
        // 向量比较与点乘避免循环
        float4 cmpVec = (float4)surface.depth > farPlaneDist;
        float4 cascadeCountVec = float4(cascadeCount > 0, cascadeCount > 1, cascadeCount > 2, cascadeCount > 3);
        cascadeIndex = dot(cmpVec, cascadeCountVec);
        if(cascadeIndex >= cascadeCount){
            // 超出级联范围不渲染阴影
            data.strength = 0;
        }
        else{
            float fade = FadedShadowStrength(surface.depth, 1.f / farPlaneDist[cascadeIndex], cascadeFade);
            if(cascadeIndex == cascadeCount - 1){
                data.strength *= fade;
            }
            else{
                // 其他级联在边界处进行混合
                data.cascadeBlend = fade;
            }
        }
    }

    data.cascadeIndex = cascadeIndex;
    
    return data;
}

float PCF(float3 posSS)
{
#if DIRECTIONAL_FILTER_SAMPLES > 1
    float visibility = 0;
    const int sampleRadius = DIRECTIONAL_FILTER_SAMPLES;
    const int area = sampleRadius * sampleRadius;
    const int halfRadius = sampleRadius / 2;
    [unroll]
    for(int i = 0; i < area; ++i) {
        int2 offset = int2(i % sampleRadius - halfRadius, i / sampleRadius - halfRadius);
        float shadow = gShadowMap.SampleCmpLevelZero(gShadowSampler, posSS.xy, posSS.z, offset);
        visibility += shadow;
    }
    return visibility / area;
#else
    return gShadowMap.SampleCmpLevelZero(gShadowSampler, posSS.xy, posSS.z);
#endif
}

float GetDirectionalShadowAttenuation(DirectionalShadowData directional, Surface surface)
{
    ShadowData shadowData = GetShadowData(surface);
    [branch]
    if(shadowData.strength <= 0)
        return 1.0;

    uint matrixIndex = directional.tileIndex + shadowData.cascadeIndex;
    float4x4 viewProj = gShadowConstants.shadowViewProjs[matrixIndex];
    // 变换到 NDC 空间
    float4 posTS = mul(float4(surface.position, 1), viewProj);

    float shadow = PCF(posTS.xyz);

    // 对不同级联之间的交界线进行过度,会造成较大性能开销
    [branch]
    if (shadowData.cascadeBlend < 1) {
        // 获取下一个级联下该像素的阴影
        posTS = mul(float4(surface.position, 1), gShadowConstants.shadowViewProjs[matrixIndex + 1]);
        // 对两个级联的结果进行插值
        shadow = lerp(PCF(posTS.xyz), shadow, shadowData.cascadeBlend);
    }

    return lerp(1.0, shadow, shadowData.strength);
}


#endif // __SHADOW_HLSLI__
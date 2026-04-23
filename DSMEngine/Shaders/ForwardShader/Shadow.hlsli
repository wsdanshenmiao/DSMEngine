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
    float strength;
    int tileIndex;
};

struct OtherShadowData
{
    float strength;
    int tileIndex;
    bool isPoint;
    float3 lightDirWS;
};

struct CascadeShadowData
{
    uint cascadeIndex;
    float strength;
    float cascadeBlend;
};

ConstantBuffer<ShaderResource::ShadowConstants> gShadowConstants : register(b3);

Texture2D<float> gDirectionalShadowMap : register(t5);
Texture2D<float> gOtherShadowMap : register(t6);
StructuredBuffer<float4x4> gDirectionalShadowViewProjs : register(t7);
StructuredBuffer<ShaderResource::OtherLightShadowData> gOtherLightShadowDatas : register(t8);

float FadedShadowStrength (float dist, float scale, float fade)
{
    return saturate((1 - dist * scale) * fade);
}

CascadeShadowData GetCascadeShadowData(Surface surface)
{
    float recMaxDistance = gShadowConstants.recMaxDistance;
    float recDistanceFade = gShadowConstants.recDistanceFade;
    float cascadeFade = gShadowConstants.cascadeFade;
    CascadeShadowData data;
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

float PCF(Texture2D<float> shadowMap, float3 posSS, float invSize, float3 bounds = float3(0, 0, 1))
{
#if DIRECTIONAL_FILTER_SAMPLES > 1
    float visibility = 0;
    const int filterSize = DIRECTIONAL_FILTER_SAMPLES;
    const int area = filterSize * filterSize;
    const int halfRadius = filterSize / 2;
    [unroll]
    for(int i = 0; i < area; ++i) {
        int2 offset = int2(i % filterSize - halfRadius, i / filterSize - halfRadius);
        float2 uv = posSS.xy + offset * invSize;
        uv = clamp(uv, bounds.xy, bounds.xy + bounds.z);
        visibility += shadowMap.SampleCmpLevelZero(gShadowSampler, uv, posSS.z, offset);
    }
    return visibility / area;
#else
    return shadowMap.SampleCmpLevelZero(gShadowSampler, posSS.xy, posSS.z);
#endif
}

float GetDirectionalShadowAttenuation(DirectionalShadowData directional, Surface surface)
{
    CascadeShadowData shadowData = GetCascadeShadowData(surface);
    [branch]
    if(shadowData.strength <= 0 || directional.strength <= 0)
        return 1.0;

    uint matrixIndex = directional.tileIndex + shadowData.cascadeIndex;
    float4x4 viewProj = gDirectionalShadowViewProjs[matrixIndex];
    // 变换到 NDC 空间
    float4 posTS = mul(float4(surface.position, 1), viewProj);

    float invMapSize = gShadowConstants.directionalShadowMapSize.y;
    float shadow = PCF(gDirectionalShadowMap, posTS.xyz, invMapSize);

    // 对不同级联之间的交界线进行过度,会造成较大性能开销
    [branch]
    if (shadowData.cascadeBlend < 1) {
        // 获取下一个级联下该像素的阴影
        posTS = mul(float4(surface.position, 1), gDirectionalShadowViewProjs[matrixIndex + 1]);
        // 对两个级联的结果进行插值
        shadow = lerp(PCF(gDirectionalShadowMap, posTS.xyz, invMapSize), shadow, shadowData.cascadeBlend);
    }

    return lerp(1.0, shadow, shadowData.strength);
}

float GetOtherShadowAttenuation(OtherShadowData other, Surface surface)
{
    [branch]
    if(other.strength <= 0)
        return 1.0;

    uint tileIndex = other.tileIndex;
    if(other.isPoint){
        tileIndex += GetCubeMapFaceIndex(-other.lightDirWS);
    }
    ShaderResource::OtherLightShadowData shadowData = gOtherLightShadowDatas[tileIndex];
    float3 shadowParams = shadowData.shadowParams.xyz;
    float4 posTS = mul(float4(surface.position, 1), shadowData.shadowMatrix);
    posTS.xyz /= posTS.w;
    float invMapSize = gShadowConstants.otherShadowMapSize.y;
    return PCF(gOtherShadowMap, posTS.xyz, invMapSize, shadowParams.xyz);
}


#endif // __SHADOW_HLSLI__
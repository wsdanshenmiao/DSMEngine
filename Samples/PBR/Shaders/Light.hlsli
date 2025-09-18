#ifndef __LIGHT_HLSLI__
#define __LIGHT_HLSLI__

#include "Surface.hlsli"
#include "BRDF.hlsli"
#include "Shadow.hlsli"

struct Light
{
    float3 color;
    float3 direction;
    float attenuation;
};

cbuffer gLightData : register(b3)
{
    int dirLightCount;
}


StructuredBuffer<DirectionalLightData> gDirLightData : register(t6);

int GetDirectionalLightCount()
{
    return dirLightCount;
}


Light GetDirectionalLight(int index, Surface surface)
{
    DirectionalShadowData shadowData;
    shadowData.tileIndex = index;
    Light light;
    light.color = gDirLightData[index].color.rgb;
    light.direction = normalize(gDirLightData[index].direction.xyz);
    light.attenuation = GetDirectionalShadowAttenuation(shadowData, surface);
    return light;
}

float3 ShadeLighting(Surface surface, Light light)
{
    float3 halfDir = normalize(light.direction + surface.viewDir);
    float NoV = saturate(dot(surface.normal, surface.viewDir));
    float NoL = saturate(dot(surface.normal, light.direction));
    float NoH = saturate(dot(surface.normal, halfDir));
    float LoH = saturate(dot(light.direction, halfDir));    // 与 VoH 相同

    float3 f0 = lerp(s_DielectricSpecular, surface.color, surface.metallic);
    float3 specular = SpecularBRDF(NoV, NoL, NoH, LoH, f0, surface.roughness);
    float3 diffuse = DiffuseBurley(NoV, NoL, LoH, surface.roughness);
    float3 diffuseCol = surface.color * (1 - surface.metallic);
    diffuse *= diffuseCol;

    float3 radians = NoL * light.color * (diffuse + specular);
    return radians * light.attenuation;
}

float3 ShadeLighting(Surface surface)
{
    float3 color = 0;
    for(int i = 0; i < GetDirectionalLightCount(); i++) {
        Light dirLight = GetDirectionalLight(i, surface);
        color += ShadeLighting(surface, dirLight);
    }
    return color;
}

#endif
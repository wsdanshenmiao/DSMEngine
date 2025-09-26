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

ConstantBuffer<LightData> gLightData : register(b3);

StructuredBuffer<DirectionalLightData> gDirLightData : register(t6);

uint GetDirectionalLightCount()
{
    return gLightData.dirLightCount;
}

uint GetOtherLightCount()
{
    return gLightData.otherLightCount;
}


Light GetDirectionalLight(uint index, Surface surface)
{
    DirectionalShadowData shadowData;
    shadowData.tileIndex = index;
    Light light;
    light.color = gDirLightData[index].color.rgb;
    light.direction = normalize(gDirLightData[index].direction.xyz);
    light.attenuation = GetDirectionalShadowAttenuation(shadowData, surface);
    return light;
}

// Light GetOtherLight(uint index, Surface surface)
// {
//     OtherLightData otherLightData = gOtherLightData[index];
//     Light light;
//     light.color = otherLightData.color.rgb;
//     light.direction = normalize(otherLightData.direction.xyz);
//     light.attenuation = GetOtherLightAttenuation(index, surface);
//     return light;
// }

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

    float3 radians = saturate(NoL * light.attenuation) * light.color * (diffuse + specular);
    return radians;
}

float3 ShadeLighting(Surface surface)
{
    float3 color = 0;
    for(uint i = 0; i < GetDirectionalLightCount(); i++) {
        Light dirLight = GetDirectionalLight(i, surface);
        color += ShadeLighting(surface, dirLight);
    }
    // for(uint ii = 0; ii < GetOtherLightCount(); ++ii){
    //     Light otherLight = GetOtherLight(ii, surface);
    //     color += ShadeLighting(surface, otherLight);
    // }
    return color;
}

#endif
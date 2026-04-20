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

ConstantBuffer<ShaderResource::LightData> gLightData : register(b2);

StructuredBuffer<ShaderResource::DirectionalLightData> gDirLightData : register(t3);
StructuredBuffer<ShaderResource::OtherLightData> gOtherLightData : register(t4);

uint GetDirectionalLightCount()
{
    return gLightData.dirLightCount;
}

uint GetOtherLightCount()
{
    return gLightData.otherLightCount;
}

// 计算光源的平方衰减
float GetSquareFalloffAttenuation(float3 posToLight, float invRange)
{
    float distSqr = dot(posToLight, posToLight);
    float factor = distSqr * invRange * invRange;
    float smoothFactor = max(1 - factor * factor, 0);
    return smoothFactor * smoothFactor / max(distSqr, 1e-4);
}

float GetSpotAngleAttenuation(float3 l, float3 lightDir, float innerAngle, float outerAngle)
{
    float cosOuter = cos(outerAngle);
    float spotScale = 1.0 / max(cos(innerAngle) - cosOuter, 1e-4);
    float spotOffset = -cosOuter * spotScale;

    float cd = dot(lightDir, l);
    float attenuation = saturate(cd * spotScale + spotOffset);
    return attenuation * attenuation;
}


DirectionalShadowData GetDirectionalShadowData(uint index)
{
    ShaderResource::DirectionalLightData lightData = gDirLightData[index];
    DirectionalShadowData shadowData;
    shadowData.strength = lightData.shadowData.x;
    shadowData.tileIndex = lightData.shadowData.y;
    return shadowData;
}

OtherShadowData GetOtherShadowData(uint index, float3 lightDirWS)
{
    ShaderResource::OtherLightData lightData = gOtherLightData[index];
    OtherShadowData shadowData;
    shadowData.strength = lightData.shadowData.x;
    shadowData.tileIndex = lightData.shadowData.y;
    shadowData.isPoint = lightData.shadowData.z != 0;
    shadowData.lightDirWS = lightDirWS;
    return shadowData;
}


Light GetDirectionalLight(uint index, Surface surface)
{
    ShaderResource::DirectionalLightData lightData = gDirLightData[index];
    Light light;
    light.color = lightData.color.rgb;
    light.direction = normalize(lightData.direction.xyz);
    light.attenuation = GetDirectionalShadowAttenuation(GetDirectionalShadowData(index), surface);
    return light;
}

Light GetOtherLight(uint index, Surface surface)
{
    ShaderResource::OtherLightData lightData = gOtherLightData[index];
    float3 pos = lightData.positionAndRange.xyz;
    float invRange = lightData.positionAndRange.w;
    float2 spotAngle = lightData.spotAngle.xy;
    Light light;
    light.color = lightData.color.rgb;
    float3 posToLight = pos - surface.position;
    float3 lightDir = normalize(posToLight);
    light.direction = lightDir;
    light.attenuation = GetSquareFalloffAttenuation(posToLight, invRange);
    light.attenuation *= GetSpotAngleAttenuation(lightDir, lightData.direction.xyz, spotAngle.x, spotAngle.y);
    light.attenuation *= GetOtherShadowAttenuation(GetOtherShadowData(index, lightDir), surface);
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
    for(uint ii = 0; ii < GetOtherLightCount(); ++ii){
        Light otherLight = GetOtherLight(ii, surface);
        color += ShadeLighting(surface, otherLight);
    }
    return color;
}

#endif
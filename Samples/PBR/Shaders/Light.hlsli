#ifndef __LIGHT_HLSLI__
#define __LIGHT_HLSLI__

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


Light GetDirectionalLight(int index)
{
    Light light;
    light.color = gDirLightData[index].color.rgb;
    light.direction = gDirLightData[index].direction.xyz;
    light.attenuation = 1.0f;
    return light;
}

#endif
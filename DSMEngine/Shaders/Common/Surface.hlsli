#ifndef __SURFACE_HLSLI__
#define __SURFACE_HLSLI__



// 物体的表面属性
struct Surface
{
    float3 position;
    float depth;
    float3 normal;
    float roughness;
    float3 color;
    float alpha;
    float3 viewDir;
    float metallic;
};

#endif
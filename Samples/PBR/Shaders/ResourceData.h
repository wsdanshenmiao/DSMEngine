#ifndef __CONSTANTBUFFERS_HLSLI__
#define __CONSTANTBUFFERS_HLSLI__


#if defined(__cplusplus)
using float3 = DSM::Math::Vector3;
using float4 = DSM::Math::Vector4;
using float3x3 = DSM::Math::Matrix3;
using float4x4 = DSM::Math::Matrix4;
#endif

#define MAX_DIRECTIONAL_LIGHT_COUNT 4
#define MAX_SHADOWED_DIRECTIONAL_LIGHT_COUNT 4

struct MeshConstants
{
    float4x4 world;
    float4x4 worldIT;
};

struct PassConstants
{
    float4x4 view;
    float4x4 viewInv;
    float4x4 proj;
    float4x4 projInv;
    float3 cameraPos;
    float deltaTime;
};


struct DirectionalLightData
{
    float4 color;
    float4 direction;
};

struct ShadowConstants
{
    float4x4 shadowViewProjs[MAX_SHADOWED_DIRECTIONAL_LIGHT_COUNT];
};

#endif
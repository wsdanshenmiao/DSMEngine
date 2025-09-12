#ifndef __CONSTANTBUFFERS_HLSLI__
#define __CONSTANTBUFFERS_HLSLI__


#if defined(__cplusplus)
using float3 = DSM::Math::Vector3;
using float4 = DSM::Math::Vector4;
using float3x3 = DSM::Math::Matrix3;
using float4x4 = DSM::Math::Matrix4;
#endif

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
    float4x4 shadowTrans;
    float3 cameraPos;
    float deltaTime;
};


#endif
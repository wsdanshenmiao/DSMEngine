#include "../ResourceData.h"
#include "../Common.hlsli"

struct Attributes
{
    float3 posOS : POSITION;
    float3 normal : NORMAL;
};

struct Varyings
{
    float4 posCS : SV_POSITION;
    float3 normal : NORMAL;
};

ConstantBuffer<MeshConstants> gMeshConstants : register(b0);
cbuffer gPassConstants : register(b1)
{
    float4x4 gView;
    float4x4 gProj;
}

// 用于 PreZ Pass 的渲染管线
Varyings GeometryPassVS(Attributes input)
{
    Varyings output;
    float4 posWS = mul(float4(input.posOS, 1.0), gMeshConstants.world);
    float3 normal = mul(normalize(input.normal), (float3x3)gMeshConstants.worldIT);
    normal = mul(normal, (float3x3)gView);
    float4x4 viewProj = mul(gView, gProj);
    output.posCS = mul(posWS, viewProj);
    output.normal = normal;
    return output;
}


float2 GeometryPassPS(Varyings input) : SV_TARGET0
{
    // 压缩法线
    return EncodeNormal(normalize(input.normal));
}
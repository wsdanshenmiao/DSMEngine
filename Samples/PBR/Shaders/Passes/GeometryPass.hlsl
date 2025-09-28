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
    float4x4 gViewProj;
}

// 用于 PreZ Pass 的渲染管线
Varyings GeometryPassVS(Attributes input)
{
    Varyings output;
    float4 posWS = mul(float4(input.posOS, 1.0), gMeshConstants.world);
    output.posCS = mul(posWS, gViewProj);
    output.normal = normalize(input.normal);
    return output;
}


float2 GeometryPassPS(Varyings input) : SV_TARGET0
{
    // 压缩法线
    return EncodeNormal(normalize(input.normal));
}
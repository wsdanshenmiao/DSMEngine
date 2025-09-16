#include "ResourceData.h"
#include "Common.hlsli"

struct Attributes
{
    float3 posOS : POSITION;
    float2 uv : TEXCOORD0;
};

struct Varyings
{
    float4 posCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

ConstantBuffer<MeshConstants> gMeshConstants : register(b0);
cbuffer gShadowPassConstants : register(b1)
{
    float4x4 gViewProj;
    float4 gMatBaseColor;
};

Texture2D<float4> gBaseColorTex : register(t0);

Varyings ShadowPassVS(Attributes input)
{
    Varyings output;

    float4 posWS = mul(float4(input.posOS, 1.0), gMeshConstants.world);
    output.posCS = mul(posWS, gViewProj);
    output.uv = input.uv;

    return output;
}


void ShadowPassPS(Varyings input)
{
    float4 baseCol = gBaseColorTex.Sample(gDefaultSampler, input.uv);
    baseCol *= gMatBaseColor;

#if defined(_SHADOWS_CLIP)
    clip(baseCol.a - 0.1);
#endif
}
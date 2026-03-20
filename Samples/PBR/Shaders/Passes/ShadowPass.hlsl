#include "../ResourceData.h"
#include "../Common.hlsli"

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

StructuredBuffer<MeshConstants> gMeshBuffer : register(t0);
StructuredBuffer<MaterialConstants> gMaterialBuffer : register(t1);

cbuffer gShadowPassConstants : register(b0)
{
    float4x4 gViewProj;
};

cbuffer gObjectConstants : register(b1)
{
    int gObjectIndex;
}

Varyings ShadowPassVS(Attributes input)
{
    Varyings output;

    MeshConstants meshConst = gMeshBuffer[gObjectIndex];
    float4 posWS = mul(float4(input.posOS, 1.0), meshConst.world);
    output.posCS = mul(posWS, gViewProj);
    output.uv = input.uv;

    return output;
}


void ShadowPassPS(Varyings input)
{
    MaterialConstants matConst = gMaterialBuffer[gObjectIndex];
    // float4 baseCol = gBaseColorTex.Sample(gAnisoWrapSampler, input.uv);
    float4 baseCol = matConst.baseColor;

#if defined(SHADOWS_CLIP)
    clip(baseCol.a - 0.1);
#endif
}
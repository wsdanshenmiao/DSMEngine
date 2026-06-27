#include "../../Common/ResourceData.h"
#include "../../Common/Common.hlsli"

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

StructuredBuffer<ShaderResource::MeshData> gMeshBuffer : register(t0);
StructuredBuffer<ShaderResource::MaterialData> gMaterialBuffer : register(t1);

Texture2D gBaseColorTex[] : register(t0, space1);

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

    ShaderResource::MeshData meshCB = gMeshBuffer[gObjectIndex];
    float4 posWS = mul(float4(input.posOS, 1.0), meshCB.world);
    output.posCS = mul(posWS, gViewProj);
    output.uv = input.uv;

    return output;
}


void ShadowPassPS(Varyings input)
{
    ShaderResource::MaterialData matData = gMaterialBuffer[gObjectIndex];
    Texture2D baseColTex = gBaseColorTex[matData.textureIndex[ShaderResource::kBaseColor]];
    float4 baseCol = baseColTex.Sample(gAnisoWrapSampler, input.uv);
    baseCol *= matData.baseColor;

#if defined(SHADOWS_CLIP)
    clip(baseCol.a - 0.1);
#endif
}

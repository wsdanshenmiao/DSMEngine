#include "ConstantBuffers.h"

struct MaterialConstants
{
    float4 baseColor;
    float4 emissiveColorAndMetallicFactor;
    float NormalTexScale;
    float RoughnessFactor;
    float pad0;
    float pad1;
};

ConstantBuffer<MeshConstants> _MeshConstants : register(b0);
ConstantBuffer<MaterialConstants> _MaterialConstants : register(b1);
ConstantBuffer<PassConstants> _PassConstants : register(b2);

// PBR相关纹理
Texture2D<float4> _BaseColorTex : register(t0);
Texture2D<float4> _DiffuseRoughnessTex : register(t1);
Texture2D<float> _MetalnessTex : register(t2);
Texture2D<float> _OcclusionTex : register(t3);
Texture2D<float3> _EmissiveTex : register(t4);
Texture2D<float3> _NormalTex : register(t5);

SamplerState defaultSampler : register(s0);

struct Attributes
{
    float3 posOS : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
#if defined(USE_TANGENT)
    float4 tangent : TANGENT;
#endif
};

struct Varyings
{
    float4 posCS : SV_POSITION;
    float3 normal : NORMAL;
#if defined(USE_TANGENT)
    float4 tangent : TANGENT;
#endif
    float2 uv : TEXCOORD0;
    float3 posWS : TEXCOORD1;
    float3 posShadow : TEXCOORD2;
};



Varyings LitPassVS(Attributes i)
{
    Varyings o;

    float4x4 viewProj = mul(_PassConstants.view, _PassConstants.proj);

    float4 posWS = mul(float4(i.posOS, 1), _MeshConstants.world);
    float3 normal = mul(i.normal, (float3x3)_MeshConstants.worldIT);
    o.posWS = posWS.xyz;
    o.posCS = mul(posWS, viewProj);
    o.uv = i.uv;
    o.normal = normalize(normal);
#if defined(USE_TANGENT)
    o.tangent.xyz = mul(i.tangent.xyz, (float3x3)_MeshConstants.worldIT).xyz;
#endif
    o.posShadow = mul(float4(o.posWS, 1), _PassConstants.shadowTrans).xyz;

    return o;
}



float4 LitPassPS(Varyings i) : SV_TARGET0
{
    float4 baseCol = _BaseColorTex.Sample(defaultSampler, i.uv);

    return baseCol;
    return float4(dot(normalize(float3(1,1,1)), normalize(i.normal)) * baseCol.rgb, baseCol.a);
}
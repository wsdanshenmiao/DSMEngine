#include "ResourceData.h"
#include "Light.hlsli"

struct MaterialConstants
{
    float4 baseColor;
    float4 emissiveColor;
    float normalTexScale;
    float metallicFactor;
    float roughnessFactor;
    float pad1;
};

ConstantBuffer<MeshConstants> gMeshConstants : register(b0);
ConstantBuffer<MaterialConstants> gMaterialConstants : register(b1);
ConstantBuffer<PassConstants> gPassConstants : register(b2);

// PBR相关纹理
Texture2D<float4> gBaseColorTex : register(t0);
Texture2D<float4> gDiffuseRoughnessTex : register(t1);
Texture2D<float> gMetalnessTex : register(t2);
Texture2D<float> gOcclusionTex : register(t3);
Texture2D<float3> gEmissiveTex : register(t4);
Texture2D<float3> gNormalTex : register(t5);

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

    float4x4 viewProj = mul(gPassConstants.view, gPassConstants.proj);

    float4 posWS = mul(float4(i.posOS, 1), gMeshConstants.world);
    float3 normal = mul(i.normal, (float3x3)gMeshConstants.worldIT);
    o.posWS = posWS.xyz;
    o.posCS = mul(posWS, viewProj);
    o.uv = i.uv;
    o.normal = normalize(normal);
#if defined(USE_TANGENT)
    o.tangent.xyz = mul(i.tangent.xyz, (float3x3)gMeshConstants.worldIT).xyz;
#endif
    o.posShadow = mul(float4(o.posWS, 1), gPassConstants.shadowTrans).xyz;

    return o;
}



float4 LitPassPS(Varyings i) : SV_TARGET0
{
    float4 baseCol = gBaseColorTex.Sample(defaultSampler, i.uv);
    baseCol *= gMaterialConstants.baseColor;
    float roughness = gDiffuseRoughnessTex.Sample(defaultSampler, i.uv).a;
    float metallic = gMetalnessTex.Sample(defaultSampler, i.uv).r;
    float occlusion = gOcclusionTex.Sample(defaultSampler, i.uv).r;
    float3 emissive = gEmissiveTex.Sample(defaultSampler, i.uv).rgb;

    // 获取视图空间的坐标
    float4 posVS = mul(float4(i.posWS, 1), gPassConstants.view);

    Surface surface;
    surface.position = i.posWS;
    surface.depth = -posVS.z;
    surface.normal = normalize(i.normal);
    surface.roughness = roughness * gMaterialConstants.roughnessFactor;
    surface.roughness = max(0.05, surface.roughness);
    surface.color = baseCol.rgb;
    surface.alpha = baseCol.a;
    surface.viewDir = normalize(gPassConstants.cameraPos - i.posWS);
    surface.metallic = metallic * gMaterialConstants.metallicFactor;

    float3 color = ShadeLighting(surface);
    color *= occlusion;
    color += emissive * gMaterialConstants.emissiveColor.rgb;
    
    return float4(color, surface.alpha);
}
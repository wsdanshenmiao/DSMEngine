#include "../../Common/ResourceData.h"
#include "../../Common/Common.hlsli"

StructuredBuffer<ShaderResource::MeshData> gMeshBuffer : register(t0);
StructuredBuffer<ShaderResource::MaterialData> gMaterialBuffer : register(t1);

ConstantBuffer<ShaderResource::PassConstants> gPassConstants : register(b0);

#if defined(USE_TANGENT)
    #define HAS_TANGENT 1
#else
    #define HAS_TANGENT 0
#endif

cbuffer ObjectConstants : register(b1)
{
    uint gObjIndex;
    uint gMaterialIndex;
}

Texture2D gTextures[] : register(t0, space1);

struct GBufferOutput
{
    float4 albedoMetallic : SV_TARGET0;
    float2 normal : SV_TARGET1;
    float4 materialAttributes : SV_TARGET2;
};

struct Attributes
{
    float3 posOS : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
#if HAS_TANGENT
    float4 tangent : TANGENT;
#endif
};

struct Varyings
{
    float4 posCS : SV_POSITION;
    float3 normalWS : NORMAL;
#if HAS_TANGENT
    float3 tangentWS : TANGENT;
    float3 bitangentWS : BITANGENT;
#endif
    float2 uv : TEXCOORD0;
    float3 posWS : TEXCOORD1;
};

Varyings GBufferPassVS(Attributes i)
{
    Varyings o;
    ShaderResource::MeshData meshData = gMeshBuffer[gObjIndex];

    float4 posWS = mul(float4(i.posOS, 1), meshData.world);
    float3 normalWS = mul(i.normal, (float3x3)meshData.worldIT);
    o.posWS = posWS.xyz;
    o.posCS = mul(posWS, mul(gPassConstants.view, gPassConstants.proj));
    o.uv = i.uv;
    o.normalWS = normalize(normalWS);
#if HAS_TANGENT
    float3 tangentWS = mul(i.tangent.xyz, (float3x3)meshData.worldIT);
    o.tangentWS = normalize(tangentWS);
    o.bitangentWS = cross(o.normalWS, o.tangentWS) * i.tangent.w;
#endif

    return o;
}

GBufferOutput GBufferPassPS(Varyings i)
{
    ShaderResource::MaterialData matData = gMaterialBuffer[gMaterialIndex];

    Texture2D baseColorTex = gTextures[matData.textureIndex[ShaderResource::kBaseColor]];
    Texture2D diffuseRoughnessTex = gTextures[matData.textureIndex[ShaderResource::kDiffuseRoughness]];
    Texture2D metalnessTex = gTextures[matData.textureIndex[ShaderResource::kMetalness]];
    Texture2D occlusionTex = gTextures[matData.textureIndex[ShaderResource::kOcclusion]];
    Texture2D emissiveTex = gTextures[matData.textureIndex[ShaderResource::kEmissive]];
    Texture2D normalTex = gTextures[matData.textureIndex[ShaderResource::kNormal]];

    float4 baseCol = baseColorTex.Sample(gAnisoWrapSampler, i.uv);
    float roughness = diffuseRoughnessTex.Sample(gAnisoWrapSampler, i.uv).g;
    float metallic = metalnessTex.Sample(gAnisoWrapSampler, i.uv).b;
    float occlusion = occlusionTex.Sample(gAnisoWrapSampler, i.uv).r;
    float3 emissive = emissiveTex.Sample(gAnisoWrapSampler, i.uv).rgb;

    baseCol *= matData.baseColor;
    roughness *= matData.roughnessFactor;
    metallic *= matData.metallicFactor;
    emissive *= matData.emissiveColor.rgb;

    // Compute view-space normal (SSAO pass expects view-space normals)
    float3 normalWS = normalize(i.normalWS);
#if HAS_TANGENT
    float3 normalMap = normalTex.Sample(gAnisoWrapSampler, i.uv).xyz * 2.0 - 1.0;
    float3 T = normalize(i.tangentWS);
    float3 B = normalize(i.bitangentWS);
    float3 N = normalWS;
    float3x3 TBN = float3x3(T, B, N);
    normalWS = normalize(mul(normalMap, TBN));
#endif

    GBufferOutput output;
    output.albedoMetallic = float4(baseCol.rgb, metallic);
    float3 normalVS = mul(normalWS, (float3x3)gPassConstants.view);
    output.normal = EncodeFloat3ToFloat2(normalize(normalVS));
    output.materialAttributes = float4(roughness, occlusion, 0, 0);
    return output;
}

#include "../ResourceData.h"
#include "../Light.hlsli"
#include "../Common.hlsli"

StructuredBuffer<ShaderResource::MeshData> gMeshBuffer : register(t0);
StructuredBuffer<ShaderResource::MaterialData> gMaterialBuffer : register(t1);
Texture2D<float> gSSAOTex : register(t2);

ConstantBuffer<ShaderResource::PassConstants> gPassConstants : register(b0);

Texture2D gTextures[] : register(t0, space1);

cbuffer ObjectConstants : register(b1)
{
    int gObjIndex;
    int gMaterialIndex;
}


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

    ShaderResource::MeshData meshData = gMeshBuffer[gObjIndex];
    float4x4 viewProj = mul(gPassConstants.view, gPassConstants.proj);

    float4 posWS = mul(float4(i.posOS, 1), meshData.world);
    float3 normal = mul(i.normal, (float3x3)meshData.worldIT);
    o.posWS = posWS.xyz;
    o.posCS = mul(posWS, viewProj);
    o.uv = i.uv;
    o.normal = normalize(normal);
#if defined(USE_TANGENT)
    o.tangent.xyz = mul(i.tangent.xyz, (float3x3)meshData.worldIT).xyz;
#endif

    return o;
}



float4 LitPassPS(Varyings i) : SV_TARGET0
{
    // 获取纹理数据
    ShaderResource::MaterialData matData = gMaterialBuffer[gMaterialIndex];
    Texture2D baseColorTex = gTextures[matData.textureIndex[ShaderResource::kBaseColor]];
    Texture2D diffuseRoughnessTex = gTextures[matData.textureIndex[ShaderResource::kDiffuseRoughness]];
    Texture2D metalnessTex = gTextures[matData.textureIndex[ShaderResource::kMetalness]];
    Texture2D occlusionTex = gTextures[matData.textureIndex[ShaderResource::kOcclusion]];
    Texture2D emissiveTex = gTextures[matData.textureIndex[ShaderResource::kEmissive]];

    float4 baseCol = baseColorTex.Sample(gAnisoWrapSampler, i.uv);
    float roughness = diffuseRoughnessTex.Sample(gAnisoWrapSampler, i.uv).g;
    float metallic = metalnessTex.Sample(gAnisoWrapSampler, i.uv).b;
    float occlusion = occlusionTex.Sample(gAnisoWrapSampler, i.uv).r;
    float3 emissive = emissiveTex.Sample(gAnisoWrapSampler, i.uv).rgb;
    
    baseCol *= matData.baseColor;
    roughness *= matData.roughnessFactor;
    metallic *= matData.metallicFactor;
    emissive *= matData.emissiveColor.rgb;

    // 获取视图空间的坐标
    float4 posVS = mul(float4(i.posWS, 1), gPassConstants.view);

    Surface surface;
    surface.position = i.posWS;
    surface.depth = posVS.z;
    surface.normal = normalize(i.normal);
    surface.roughness = max(0.05, roughness * roughness);  // 感知上的粗糙度
    surface.color = baseCol.rgb;
    surface.alpha = baseCol.a;
    surface.viewDir = normalize(gPassConstants.cameraPos - i.posWS);
    surface.metallic = metallic;

    float3 color = ShadeLighting(surface);

    float2 uv = i.posCS.xy * gPassConstants.renderTargetSize.zw;
    float ssao = gSSAOTex.Sample(gAnisoWrapSampler, uv).r;
    color *= occlusion * ssao;
    color += emissive;

    return float4(color, surface.alpha);
}
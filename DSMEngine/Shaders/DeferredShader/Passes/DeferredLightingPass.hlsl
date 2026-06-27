#include "../../Common/ResourceData.h"
#include "../../Common/Common.hlsli"
#include "../../Common/Surface.hlsli"
#include "../../Common/Light.hlsli"

ConstantBuffer<ShaderResource::PassConstants> gPassConstants : register(b0);

// GBuffer textures - use high register slots to avoid conflict with Light.hlsli
Texture2D<float4> gAlbedoMetallicTex : register(t10);
Texture2D<float2> gNormalTex : register(t11);
Texture2D<float4> gMaterialAttribTex : register(t12);
Texture2D<float> gDepthTex : register(t13);
Texture2D<float> gSSAOTex : register(t14);
StructuredBuffer<ShaderResource::TileInfo> gTileInfos : register(t15);

float4 DeferredLightingVS(uint vertexID : SV_VERTEXID) : SV_POSITION
{
    float4 posCS;
    float2 uv;
    GetFullscreenTriangle(vertexID, posCS, uv, false);
    return posCS;
}

float4 DeferredLightingPS(float4 posCS : SV_POSITION) : SV_TARGET0
{
    float2 uv = posCS.xy * gPassConstants.renderTargetSize.zw;

    // Sample GBuffer
    float4 albedoMetallic = gAlbedoMetallicTex.Sample(gPointClampSampler, uv);
    float3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    float2 encodedNormal = gNormalTex.Sample(gPointClampSampler, uv);
    float4 materialAttrib = gMaterialAttribTex.Sample(gPointClampSampler, uv);
    float roughnessPerceptual = materialAttrib.r;
    float occlusion = materialAttrib.g;
    float emissiveIntensity = materialAttrib.b;
    float depth = gDepthTex.Sample(gPointClampSampler, uv);

    // Reconstruct view-space position from depth
    float4 posVS = GetViewPosition(uv, gPassConstants.projInv, depth);
    float3 viewDir = normalize(gPassConstants.cameraPos - mul(posVS, gPassConstants.viewInv).xyz);
    float3 normalVS = DecodeFloat2ToFloat3(encodedNormal);
    float3 normalWS = mul(normalVS, (float3x3)gPassConstants.viewInv);
    normalWS = normalize(normalWS);

    // Reconstruct world position
    float4 posWS = mul(posVS, gPassConstants.viewInv);
    posWS /= posWS.w;

    // Construct surface
    Surface surface;
    surface.position = posWS.xyz;
    surface.depth = posVS.z;
    surface.normal = normalWS;
    surface.roughness = max(0.05, roughnessPerceptual * roughnessPerceptual);
    surface.color = albedo;
    surface.alpha = 1.0;
    surface.viewDir = viewDir;
    surface.metallic = metallic;

    uint tileSize = 16;
    uint dispatchWidth = (gPassConstants.renderTargetSize.x + tileSize - 1) / tileSize;
    uint2 tileCoord = uint2(posCS.xy) / tileSize;
    uint tileIndex = tileCoord.y * dispatchWidth + tileCoord.x;
    ShaderResource::TileInfo tileInfo = gTileInfos[tileIndex];

    float3 color = ShadeLighting(surface, tileInfo);

    float ssao = gSSAOTex.Sample(gPointClampSampler, uv).r;
    color *= occlusion * ssao;
    color += emissiveIntensity * albedo;

    return float4(color, 1.0);
}

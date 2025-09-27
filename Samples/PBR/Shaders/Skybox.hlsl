#include "Common.hlsli"


cbuffer SkyboxConstants : register(b0)
{
    float4x4 gInvViewProj;
};

TextureCube<float3> gSkyboxTex : register(t0);

struct Varyings
{
    float4 posCS : SV_POSITION;
    float3 viewDir : TEXCOORD0;
};

/*
 *  1
 *  |\
 *  | \
 *  |  \
 *  |___ \
 *  0       2
 */
// 输入三个顶点，构造全屏三角形
Varyings SkyboxVS(uint vertexID : SV_VertexID)
{
    Varyings o;

    float2 screenPos = float2(uint2(vertexID, vertexID << 1) & 2);
    float4 posCS = float4(screenPos * 2 - 1, 1, 1);
    o.posCS = posCS;
    float4 posVS = mul(posCS, gInvViewProj);
    o.viewDir = posVS.xyz;

    return o;
}

float4 SkyboxPS(Varyings i) : SV_TARGET0
{
    float3 dir = normalize(i.viewDir);
    float3 color = gSkyboxTex.Sample(gDefaultSampler, dir).rgb;
    return float4(color, 1);
}
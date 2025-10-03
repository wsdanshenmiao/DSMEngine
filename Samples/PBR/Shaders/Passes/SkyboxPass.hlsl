#include "../Common.hlsli"
#include "../ResourceData.h"


ConstantBuffer<SkyboxConstants> gSkyboxConstants : register(b0);

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

    // 获取裁剪空间坐标及视图方向
    float2 screenPos = float2(uint2(vertexID, vertexID << 1) & 2);
    float4 posCS = float4(screenPos * 2 - 1, gSkyboxConstants.isReversedZ ? 0 : 1, 1);
    o.posCS = posCS;
    float4 posVS = mul(posCS, gSkyboxConstants.invViewProj);
    o.viewDir = posVS.xyz;

    return o;
}

float4 SkyboxPS(Varyings i) : SV_TARGET0
{
    float3 color = gSkyboxTex.Sample(gAnisoWrapSampler, normalize(i.viewDir)).rgb;
    return float4(color, 1);
}
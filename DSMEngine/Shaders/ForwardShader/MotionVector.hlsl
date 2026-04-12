#include "Common.hlsli"


struct Varyings
{
    float4 posCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer gMotionVectorConstants : register(b0)
{
    float4x4 gCurrInvViewProj;
    float4x4 gPrevViewProj;
}

Texture2D<float> gDepthTex : register(t0);

Varyings MotionVectorVS(uint vertexID : SV_VertexID)
{
    Varyings output;
    GetFullscreenTriangle(vertexID, output.posCS, output.uv);
    return output;
}


float2 MotionVectorPS(Varyings input) : SV_TARGET0
{
    // 从深度图重构当前像素的世界位置
    float depth = gDepthTex.Sample(gPointClampSampler, input.uv);
    float4 posWS = GetWorldPosition(input.uv, gCurrInvViewProj, depth);

    // 将当前世界位置变换到上一帧的裁剪空间
    float4 prevPosCS = mul(posWS, gPrevViewProj);
    // 透视除法得到 NDC 坐标
    prevPosCS /= prevPosCS.w;

    float2 prevUV = prevPosCS.xy * 0.5f + 0.5f;
    prevUV.y = 1.0f - prevUV.y;

    return input.uv - prevUV;
}
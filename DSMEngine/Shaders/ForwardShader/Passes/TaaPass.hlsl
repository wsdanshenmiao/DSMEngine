#ifndef __TAAPASS_CS_HLSL__
#define __TAAPASS_CS_HLSL__

#include "../Common.hlsli"


struct Varyings
{
    float4 posCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};


cbuffer gTaaConstants : register(b0)
{
	float gHistoryWeight;
	float gVarianceClip;
}

Texture2D<float4> gCurrColor : register(t0);
Texture2D<float4> gHistoryColor : register(t1);
Texture2D<float2> gMotionVec : register(t2);

Varyings TaaPassVS(uint vertexID : SV_VertexID)
{
    Varyings output;
    GetFullscreenTriangle(vertexID, output.posCS, output.uv);
    return output;
}


float4 TaaPassPS(Varyings input) : SV_TARGET
{
    float4 currCol = gCurrColor.Sample(gLinearClampSampler, input.uv);

    float2 motionVec = gMotionVec.Sample(gPointClampSampler, input.uv);
    float2 prevUV = input.uv - motionVec;
    // 判断纹理是否在有效范围内，如果不在范围内则不使用历史颜色
    float2 inRange = step(0, prevUV) * step(prevUV, 1);
    float uvValid = inRange.x * inRange.y;
    prevUV = saturate(prevUV);
    float4 histCol = gHistoryColor.Sample(gLinearClampSampler, prevUV);
    
    // 比较当前颜色和历史颜色的差异，如果差异过大则降低历史权重，避免鬼影
    float3 colorDiff = abs(currCol.rgb - histCol.rgb);
    float maxDiff = max(colorDiff.r, max(colorDiff.g, colorDiff.b));
    float rejection = saturate(maxDiff * gVarianceClip);
    float historyWeight = gHistoryWeight * (1.0f - rejection) * uvValid;

    float3 finalCol = lerp(currCol.rgb, histCol.rgb, historyWeight);

    return float4(finalCol, currCol.a);
}


#endif // __TAAPASS_CS_HLSL__

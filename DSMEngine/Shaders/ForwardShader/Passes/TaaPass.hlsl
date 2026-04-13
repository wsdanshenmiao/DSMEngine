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
	uint gResetHistory;
    float pad;
}

Texture2D<float4> gCurrColor : register(t0);
Texture2D<float4> gHistoryColor : register(t1);
Texture2D<float> gMotionVec : register(t2);

Varyings TaaPassVS(uint vertexID : SV_VertexID)
{
    Varyings output;
    GetFullscreenTriangle(vertexID, output.posCS, output.uv);
    return output;
}


float4 TaaPassPS(Varyings input) : SV_TARGET
{
    float4 currCol = gCurrColor.Sample(gLinearClampSampler, input.uv);

    // 重置历史帧
    if(gResetHistory != 0){
        return currCol;
    }

    float2 motionVec = gMotionVec.Sample(gPointClampSampler, input.uv);
    float2 prevUV = input.uv - motionVec;
    float2 inRange = step(0, prevUV) * step(prevUV, 1);
    float uvValid = inRange.x * inRange.y;
    prevUV = saturate(prevUV);
    float4 histCol = gHistoryColor.Sample(gLinearClampSampler, prevUV);
    
    float3 colorDiff = abs(currCol.rgb - histCol.rgb);
    float maxDiff = max(colorDiff.r, max(colorDiff.g, colorDiff.b));
    float rejection = saturate(maxDiff * gVarianceClip);
    float historyWeight = gHistoryWeight * (1.0f - rejection) * uvValid;

    float3 finalCol = lerp(currCol.rgb, histCol.rgb, historyWeight);

    return float4(finalCol, currCol.a);
}


#endif // __TAAPASS_CS_HLSL__

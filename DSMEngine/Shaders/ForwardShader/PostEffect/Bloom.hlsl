#ifndef __BLOOMPASS_HLSL__
#define __BLOOMPASS_HLSL__

#include "../Common.hlsli"

#define THREAD_SIZE 8

cbuffer gBloomConstants : register(b0)
{
    float gThreshold;
}

RWTexture2D<float4> gDstTex : register(u0);
Texture2D<float4> gSrcTex : register(t0);

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void BloomExtractCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    gDstTex.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) {
        return;
    }

    float3 col = gSrcTex.Load(int3(dispatchThreadID.xy, 0)).rgb;
    gDstTex[dispatchThreadID.xy] = Luminance(col) > gThreshold ? float4(col, 1) : float4(0, 0, 0, 1);
}

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void BloomCompositeCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    gDstTex.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) {
        return;
    }

    float4 originCol = gSrcTex.Load(int3(dispatchThreadID.xy, 0));
    gDstTex[dispatchThreadID.xy] += originCol;
}



#endif // __BLOOMPASS_HLSL__

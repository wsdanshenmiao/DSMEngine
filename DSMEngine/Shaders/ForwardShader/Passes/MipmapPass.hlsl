#ifndef __MIPMAPPASS_HLSL__
#define __MIPMAPPASS_HLSL__

#include "../Common.hlsli"

#define THREAD_SIZE 8

cbuffer MipmapConstants : register(b0)
{
	uint gDstWidth;
	uint gDstHeight;
}

RWTexture2D<float4> gDstTex : register(u0);
Texture2D<float4> gSrcTex : register(t0);

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void GenerateMipmapCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	if (dispatchThreadID.x >= gDstWidth || dispatchThreadID.y >= gDstHeight) {
		return;
	}
    float2 srcCoord = (dispatchThreadID.xy + 0.5f) / float2(gDstWidth, gDstHeight);
    // 使用双线性过滤来采样源纹理，直接获取四个像素的插值结果
	gDstTex[dispatchThreadID.xy] = gSrcTex.SampleLevel(gLinearClampSampler, srcCoord, 0);
}

#endif // __MIPMAPPASS_HLSL__

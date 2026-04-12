#ifndef __TONEMAPPINGPASS_HLSL__
#define __TONEMAPPINGPASS_HLSL__

#include "../Common.hlsli"
#include "../ToneMappingCommon.hlsli"

#define THREAD_SIZE 8


#ifndef TONEMAP_REINHARD
#define TONEMAP_REINHARD 0
#endif

#ifndef TONEMAP_REINHARD_EXTENDED
#define TONEMAP_REINHARD_EXTENDED 1
#endif

#ifndef TONEMAP_ACES_FITTED
#define TONEMAP_ACES_FITTED 2
#endif

#ifndef TONEMAP_ACES_APPROX
#define TONEMAP_ACES_APPROX 3
#endif

#ifndef TONEMAP_UNCHARTED2
#define TONEMAP_UNCHARTED2 4
#endif

#ifndef TONEMAP_TYPE
#define TONEMAP_TYPE TONEMAP_ACES_FITTED
#endif

#ifndef TONEMAP_APPLY_GAMMA
#define TONEMAP_APPLY_GAMMA 1
#endif

cbuffer gToneMappingConstants : register(b0)
{
	float gExposure;
    float gWhitePoint;  // 仅在使用 ReinhardExtended 时有效
}

RWTexture2D<float4> gDstTex : register(u0);
Texture2D<float4> gSrcTex : register(t0);


float3 ApplyToneMapping(float3 color)
{
#if TONEMAP_TYPE == TONEMAP_REINHARD
	return ToneMapReinhard(color);
#elif TONEMAP_TYPE == TONEMAP_REINHARD_EXTENDED
    return ToneMapReinhardExtended(color, gWhitePoint);
#elif TONEMAP_TYPE == TONEMAP_ACES_FITTED
	return ToneMapACESFitted(color);
#elif TONEMAP_TYPE == TONEMAP_ACES_APPROX
    return ToneMapACESApprox(color);
#elif TONEMAP_TYPE == TONEMAP_UNCHARTED2
	return ToneMapUncharted2(color);
#else
	return ToneMapACESFitted(color);
#endif
}

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void ToneMappingCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint width, height;
	gDstTex.GetDimensions(width, height);
	if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) {
		return;
	}

	float4 src = gSrcTex.Load(int3(dispatchThreadID.xy, 0));
	float3 mapped = ApplyToneMapping(max(src.rgb * gExposure, 0.0f));

#if TONEMAP_APPLY_GAMMA
	mapped = LinearToSRGB(mapped);
#endif

	gDstTex[dispatchThreadID.xy] = float4(mapped, src.a);
}


#endif // __TONEMAPPINGPASS_HLSL__

#ifndef __TONEMAPPINGPASS_HLSL__
#define __TONEMAPPINGPASS_HLSL__

#include "../Common.hlsli"

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

// 使用 Reinhard 算法进行映射
float3 ToneMapReinhard(float3 color)
{
	return color / (1.0f + color);
}

// 使用白点的 Reinhard 算法进行映射
float3 ToneMapReinhardExtended(float3 color)
{
    float3 numerator = color * (1.0f + (color / (gWhitePoint * gWhitePoint)));
    return numerator / (1.0f + color);
}


// ACES RRT + ODT 拟合函数
float3 ACESRRTAndODTFit(float3 color)
{
	float3 a = color * (color + 0.0245786f) - 0.000090537f;
	float3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
	return a / b;
}

float3 ToneMapACESFitted(float3 color)
{
    // 从线性空间变换到 ACEScg 空间
	static const float3x3 inputMat = {
		{0.59719f, 0.35458f, 0.04823f},
		{0.07600f, 0.90834f, 0.01566f},
		{0.02840f, 0.13383f, 0.83777f}
	};
    // 从 ACEScg 空间变换到线性空间
	static const float3x3 outputMat = {
		{1.60475f, -0.53108f, -0.07367f},
		{-0.10208f, 1.10813f, -0.00605f},
		{-0.00327f, -0.07276f, 1.07602f}
	};

	color = mul(inputMat, color);
	color = ACESRRTAndODTFit(color);
	color = mul(outputMat, color);
	return saturate(color);
}


// Krzysztof Narkowicz 提供的近似 ACES 拟合
float3 ToneMapACESApprox(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}


// 神秘海域2 的色调映射曲线
float3 Uncharted2TonemapCurve(float3 x)
{
	const float A = 0.15f;
	const float B = 0.50f;
	const float C = 0.10f;
	const float D = 0.20f;
	const float E = 0.02f;
	const float F = 0.30f;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 ToneMapUncharted2(float3 color)
{
	const float3 W = 11.2f;
	float3 curr = Uncharted2TonemapCurve(color);
	float3 whiteScale = rcp(Uncharted2TonemapCurve(W));
	return saturate(curr * whiteScale);
}


float3 ApplyToneMapping(float3 color)
{
#if TONEMAP_TYPE == TONEMAP_REINHARD
	return ToneMapReinhard(color);
#elif TONEMAP_TYPE == TONEMAP_REINHARD_EXTENDED
    return ToneMapReinhardExtended(color);
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

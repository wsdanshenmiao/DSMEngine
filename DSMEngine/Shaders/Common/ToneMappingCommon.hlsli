#ifndef __TONEMAPPINGCOMMON_HLSL__
#define __TONEMAPPINGCOMMON_HLSL__

// 使用 Reinhard 算法进行映射
float3 ToneMapReinhard(float3 color)
{
	return color / (1.0f + color);
}

// 使用白点的 Reinhard 算法进行映射
float3 ToneMapReinhardExtended(float3 color, float whitePoint)
{
    float3 numerator = color * (1.0f + (color / (whitePoint * whitePoint)));
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


#endif // __TONEMAPPINGCOMMON_HLSL__
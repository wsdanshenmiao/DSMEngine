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
    float2 gTexelSize;
}

Texture2D<float4> gCurrColor : register(t0);
Texture2D<float4> gHistoryColor : register(t1);
Texture2D<float2> gMotionVec : register(t2);
Texture2D<float> gDepthTex : register(t3);


bool CmpZGreater(float z1, float z2)
{
#if defined(REVERSED_Z)
    return z1 < z2;
#else
    return z1 > z2;
#endif
}

float4 SampleColor(Texture2D<float4> texture, float2 uv, int2 offset = 0)
{
    float4 color = texture.SampleLevel(gLinearClampSampler, uv, 0, offset);
#if defined(USE_YCOCG)
    return float4(RGBToYCoCg(color.rgb), color.a);
#else
    return color;
#endif
}

float4 ResolveColor(float4 color)
{
#if defined(USE_YCOCG)
    return float4(YCoCgToRGB(color.rgb), color.a);
#else
    return color;
#endif
}

// 将历史帧的颜色限制在当前帧颜色的一个范围内
void GetColorMinMax(float3 color, float3 historyCol, float2 uv, out float3 colorMin, out float3 colorMax)
{
    colorMin = color;
    colorMax = color;

    [unroll]
    for(int i = -1; i <= 1; ++i) {
        [unroll]
        for(int j = -1; j <= 1; ++j){
            if(i == 0 && j == 0) 
                continue;
            float3 color = SampleColor(gCurrColor, uv, int2(i, j)).rgb;
            colorMin = min(colorMin, color);
            colorMax = max(colorMax, color);
        }
    }
}

float3 ClampColor(float3 color, float3 historyCol, float2 uv)
{
    float3 colorMin, colorMax;
    GetColorMinMax(color, historyCol, uv, colorMin, colorMax);
    return clamp(historyCol, colorMin, colorMax);
}

float3 ClipColor(float3 color, float3 historyCol, float2 uv)
{
    float3 colorMin, colorMax;
    GetColorMinMax(color, historyCol, uv, colorMin, colorMax);

    float3 rayOrigin = historyCol;
    float3 rayDir = color - historyCol;
    float3 invDir = rcp(rayDir);

    // 计算历史颜色到当前颜色的包围盒与射线的交点，将历史颜色限制在当前颜色的范围内
    float3 tMin = (colorMin - rayOrigin) * invDir;
    float3 tMax = (colorMax - rayOrigin) * invDir;
    float3 tEnter = min(tMin, tMax);
    float time = max(max(tEnter.x, tEnter.y), tEnter.z);
    time = saturate(time);

    return lerp(historyCol, color, time);
}

float2 FindClosestFragment3x3(float2 uv)
{
    float2 du = float2(gTexelSize.x, 0);
    float2 dv = float2(0, gTexelSize.y);

    float centerDepth = gDepthTex.Sample(gPointClampSampler, uv);
    float3 cloest = float3(uv, centerDepth);
    [unroll]
    for(int i = -1; i <= 1; ++i) {
        [unroll]
        for(int j = -1; j <= 1; ++j) {
            if(i == 0 && j == 0) 
                continue;

            float2 sampleUV = uv + du * i + dv * j;
            float depth = gDepthTex.Sample(gPointClampSampler, sampleUV);
            if(CmpZGreater(cloest.z, depth)) {
                cloest = float3(sampleUV, depth);
            }
        }
    }

    return cloest.xy;
}

Varyings TaaPassVS(uint vertexID : SV_VertexID)
{
    Varyings output;
    GetFullscreenTriangle(vertexID, output.posCS, output.uv);
    return output;
}

float4 TaaPassPS(Varyings input) : SV_TARGET
{
    float4 currCol = SampleColor(gCurrColor, input.uv);

#if defined(USE_CLOSEST_FRAGMENT)
    float2 uv = FindClosestFragment3x3(input.uv);
#else
    float2 uv = input.uv;
#endif
    float2 motionVec = gMotionVec.Sample(gPointClampSampler, uv);
    float2 prevUV = input.uv - motionVec;

    // 从历史帧中采样颜色，并进行颜色限制
    float4 histCol = SampleColor(gHistoryColor, prevUV);
#if defined(USE_COLOR_CLIP)
    histCol.rgb = ClipColor(currCol.rgb, histCol.rgb, input.uv);
#else
    histCol.rgb = ClampColor(currCol.rgb, histCol.rgb, input.uv);
#endif    

    // 根据运动矢量的长度计算拒绝率
    float historyWeight = saturate(gHistoryWeight - length(motionVec) * gVarianceClip);

    currCol = ResolveColor(currCol);
    histCol = ResolveColor(histCol);
    float3 finalCol = lerp(currCol.rgb, histCol.rgb, historyWeight);

    return float4(finalCol, currCol.a);
}


#endif // __TAAPASS_CS_HLSL__

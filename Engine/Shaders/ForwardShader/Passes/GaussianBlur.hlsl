#ifndef __GAUSSIANBLUR__HLSL__
#define __GAUSSIANBLUR__HLSL__

#include "../../Common/Common.hlsli"
#include "../../Common/ResourceData.h"

// 一个线程组中有 256 个线程
#define THREAD_SIZE 256

// 最大模糊半径
static const uint sMaxBlurRadius = 10;

cbuffer gBlurConstants : register(b0)
{
    uint gBlurRadius;
    bool gIsHorizontal;
};

RWTexture2D<float4> gOutput : register(u0);
Texture2D<float4> gInputTexture : register(t0);
StructuredBuffer<float> gBlurWeights : register(t1);


// 使用共享内存缓存采样的结果，避免重复的采样
groupshared float4 gCache[THREAD_SIZE + 2 * sMaxBlurRadius];

[numthreads(THREAD_SIZE, 1, 1)]
void GaussianBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
    uint blurRadius = clamp(gBlurRadius, 0, sMaxBlurRadius);
    bool isHorizontal = gIsHorizontal;
    int width, height;
    gInputTexture.GetDimensions(width, height);
    bool isValidThread = dispatchThreadID.x < (isHorizontal ? width : height) &&
        dispatchThreadID.y < (isHorizontal ? height : width);

    // 线程组边缘的像素也需要模糊，因此需要在共享内存中额外储存线程组边界外的值
    if (groupThreadID.x < blurRadius) {
        // 若纹理的大小不能被线程数整除，会有多余的线程，需要进行索引限制
        int index = max(0, dispatchThreadID.x - blurRadius);
        float2 clampUV = isHorizontal ? float2(index, dispatchThreadID.y) : float2(dispatchThreadID.y, index);
        gCache[groupThreadID.x] = gInputTexture[uint2(clampUV)];
    }
    if (groupThreadID.x >= THREAD_SIZE - blurRadius) {
        int index = min(isHorizontal ? (width - 1) : (height - 1), dispatchThreadID.x + blurRadius);
        float2 clampUV = isHorizontal ? float2(index, dispatchThreadID.y) : float2(dispatchThreadID.y, index);
        gCache[groupThreadID.x + blurRadius * 2] = gInputTexture[uint2(clampUV)];
    }

    // 当前线程负责的采样
    float2 uvHorizontal = float2(min(dispatchThreadID.xy, uint2(width, height) - 1));
    float2 uvVertical = float2(min(dispatchThreadID.yx, uint2(width, height) - 1));
    float2 uv = isHorizontal ? uvHorizontal : uvVertical;
    gCache[groupThreadID.x + blurRadius] = gInputTexture[uint2(uv)];

    // 等待所有的线程完成采样
    GroupMemoryBarrierWithGroupSync();

    if (isValidThread) {
        // 进行模糊
        float4 color = 0;
        float weightSum = 0;
        for (int i = 0; i <= blurRadius * 2; ++i) {
            float weight = gBlurWeights[i];
            color += gCache[groupThreadID.x + i] * weight;
            weightSum += weight;
        }
        color /= weightSum;

        gOutput[isHorizontal ? dispatchThreadID.xy : dispatchThreadID.yx] = color;
    }
}

#endif

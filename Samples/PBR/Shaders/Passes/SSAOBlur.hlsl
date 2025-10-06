#ifndef __SSAOBLUR__HLSL__
#define __SSAOBLUR__HLSL__

#include "../Common.hlsli"
#include "../ResourceData.h"

// 一个线程组中有 256 个线程
#define THREAD_SIZE 256

// 最大模糊半径
static const uint sMaxBlurRadius = 5;

ConstantBuffer<SSAOBlurConstants> gSSAOBlurConstants : register(b0);

RWTexture2D<float4> gOutput : register(u0);
Texture2D<float> gBlurTexture : register(t0);
// 用于判断是否需要模糊
Texture2D<float2> gNormalTexture : register(t1);
Texture2D<float> gDepthTexture : register(t2);
StructuredBuffer<float> gBlurWeights : register(t3);


// 使用共享内存缓存采样的结果，避免重复的采样
// float: AO, float2: Normal and float: Depth
groupshared float4 gCache[THREAD_SIZE + 2 * sMaxBlurRadius];

void StoreToCache(uint index, float2 uv, float2 invTexelSize)
{
    float ao = gBlurTexture[uint2(uv)];
    uv *= invTexelSize;
    float2 normal = gNormalTexture.SampleLevel(gPointBorderSampler, uv, 0);
    float depth = gDepthTexture.SampleLevel(gPointBorderSampler, uv, 0);
    gCache[index] = float4(ao, normal, depth);
}

[numthreads(THREAD_SIZE, 1, 1)]
void SSAOBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
    int blurRadius = gSSAOBlurConstants.blurRadius;
    bool isHorizontal = gSSAOBlurConstants.isHorizontal;
    int width, height;
    gBlurTexture.GetDimensions(width, height);

    float2 invTexelSize = 1.0 / float2(width, height);

    // 线程组边缘的像素也需要模糊，因此需要在共享内存中额外储存线程组边界外的值
    if (groupThreadID.x < blurRadius) {
        // 若纹理的大小不能被线程数整除，会有多余的线程，需要进行索引限制
        int index = max(0, dispatchThreadID.x - blurRadius);
        float2 clampUV = isHorizontal ? float2(index, dispatchThreadID.y) : float2(dispatchThreadID.y, index);
        StoreToCache(groupThreadID.x, clampUV, invTexelSize);
    }
    if (groupThreadID.x >= THREAD_SIZE - blurRadius) {
        int index = min(isHorizontal ? (width - 1) : (height - 1), dispatchThreadID.x + blurRadius);
        float2 clampUV = isHorizontal ? float2(index, dispatchThreadID.y) : float2(dispatchThreadID.y, index);
        StoreToCache(groupThreadID.x + blurRadius * 2, clampUV, invTexelSize);
    }

    // 当前线程负责的采样
    float2 uvHorizontal = float2(min(dispatchThreadID.xy, uint2(width, height) - 1));
    float2 uvVertical = float2(min(dispatchThreadID.yx, uint2(width, height) - 1));
    float2 uv = isHorizontal ? uvHorizontal : uvVertical;
    StoreToCache(groupThreadID.x + blurRadius, uv, invTexelSize);

    // 等待所有的线程完成采样
    GroupMemoryBarrierWithGroupSync();

    float4 val = gCache[groupThreadID.x + blurRadius];
    float centerAO = val.x;
    float3 centerNormal = DecodeFloat2ToFloat3(val.yz);
    float centerDepth = GetLinearDepth(val.w, gSSAOBlurConstants.proj);

    // 进行模糊
    float ssao = centerAO * gBlurWeights[blurRadius + 1];
    float weightSum = gBlurWeights[blurRadius + 1];
    for (int i = 0; i <= blurRadius * 2; ++i) {
        if(i == blurRadius) continue;    // 中心点已经采样过

        float4 neighborVal = gCache[groupThreadID.x + i];
        float3 normal = DecodeFloat2ToFloat3(neighborVal.yz);
        float depth = GetLinearDepth(neighborVal.w, gSSAOBlurConstants.proj);

        // 只有在法线和深度相近的情况下才进行模糊
        if(dot(centerNormal, normal) > 0.8 && abs(centerDepth - depth) < 0.2){
            float weight = gBlurWeights[i];
            ssao += neighborVal.x * weight;
            weightSum += weight;
        }
    }
    ssao /= weightSum;

    gOutput[isHorizontal ? dispatchThreadID.xy : dispatchThreadID.yx] = ssao;
}

#endif
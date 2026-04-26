#include "ResourceData.h"
#include "Common.hlsli"


#ifndef TILE_SIZE
#define TILE_SIZE 16
#endif

#define THREAD_COUNT_PER_TILE (TILE_SIZE * TILE_SIZE)

RWStructuredBuffer<ShaderResource::TileInfo> gTileInfoBuffer : register(u0);
StructuredBuffer<BoundingBox> gLightBounds : register(t0);
Texture2D<float> gDepthTexture : register(t1);

ConstantBuffer<ShaderResource::TileBasedLightingConstants> gTileBasedLightingCB : register(b0);

groupshared uint gsNearPlane;
groupshared uint gsFarPlane;
groupshared uint gsTileNumLights;
groupshared ShaderResource::TileInfo gsTileInfo;

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void TileBasedLightingCS(uint3 dispatchThreadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    float4x4 proj = gTileBasedLightingCB.proj;
    float2 screenSize = gTileBasedLightingCB.screenSizeAndCameraNearFar.xy;
    float2 cameraNearFar = gTileBasedLightingCB.screenSizeAndCameraNearFar.zw;

    if(groupIndex == 0){
        gsNearPlane = 0x7f7fffff;
        gsFarPlane = 0;
        gsTileNumLights = 0;
    }

    // 写入组内数据后进行同步
    GroupMemoryBarrierWithGroupSync();

    // 计算组内的最大值与最小值
    float minDepth = cameraNearFar.y;
    float maxDepth = cameraNearFar.x;
    float depth = GetLinearDepth(gDepthTexture[dispatchThreadID.xy], proj);
    [flatten]
    if(cameraNearFar.x <= depth && depth < cameraNearFar.y){
        minDepth = min(minDepth, depth);
        maxDepth = max(maxDepth, depth);
    }
    if(minDepth <= maxDepth){
        InterlockedMin(gsNearPlane, asuint(minDepth));
        InterlockedMax(gsFarPlane, asuint(maxDepth));
    }

    GroupMemoryBarrierWithGroupSync();

    // 获取当前线程组的视锥体
    float nearPlane = asfloat(gsNearPlane);
    float farPlane = asfloat(gsFarPlane);
    float2 tileScale = screenSize / TILE_SIZE;
    Frustum frustum = GetFrustumFromGroup(groupID.xy, nearPlane, farPlane, tileScale, proj);

    // 将所有的光源分配给所有的组内线程
    uint lightCount = gTileBasedLightingCB.lightCount;
    uint lightCountPerThread = (lightCount + THREAD_COUNT_PER_TILE - 1) / THREAD_COUNT_PER_TILE;
    uint begin = groupIndex * lightCountPerThread;
    uint end = min((groupIndex + 1) * lightCountPerThread, lightCount);
    uint maskIndex = begin >> 5; // 每32个光源使用一个uint来存储
    uint bitOffset = begin & 31;
    for(uint index = begin; index < end; index++){
        // 判断光源是否与瓦片相交
        if(frustum.Intersects(gLightBounds[index])){
            uint lightIndex;
            InterlockedAdd(gsTileNumLights, 1, lightIndex);
            if(lightIndex < MAX_LIGHT_COUNT_PER_TILE){
                // 使用位运算将光源索引写入瓦片信息中
                InterlockedOr(gsTileInfo.lightMask[maskIndex], (1u << bitOffset));
            }
        }
        bitOffset++;
        [flatten]
        if(bitOffset == 32){
            bitOffset = 0;
            maskIndex++;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    uint groupCountX = (screenSize.x + TILE_SIZE - 1) / TILE_SIZE;
    uint tileIndex = groupID.y * groupCountX + groupID.x;
    if(groupIndex == 0){
        gTileInfoBuffer[tileIndex] = gsTileInfo;
    }
}
#include "../Common.hlsli"
#include "../ResourceData.h"

struct Attributes
{
    float3 posOS : POSITION;
};

struct VaryingsFullScreen
{
    float4 posCS : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct Varyings
{
    float4 currPosCS : SV_POSITION;
    float3 currPos : TEXCOORD0;
    float3 prevPos : TEXCOORD1; // 存储上一帧的位置，用于计算运动矢量
};

cbuffer gMotionVectorConstants : register(b0)
{
    float4x4 gCurrMatrix;
    float4x4 gPrevViewProj;
}
Texture2D<float> gDepthTex : register(t0);


cbuffer gMotionVectorObjectConstants : register(b1)
{
    int gObjIndexOrReverseZFlag;
    int gLastFrameObjIndex;
}
StructuredBuffer<ShaderResource::MeshData> gMeshBuffer : register(t0);
StructuredBuffer<ShaderResource::MeshData> gLastFrameMeshBuffer : register(t1);

VaryingsFullScreen MotionVectorFullScreenVS(uint vertexID : SV_VertexID)
{
    VaryingsFullScreen output;
    GetFullscreenTriangle(vertexID, output.posCS, output.uv);
    return output;
}

float2 MotionVectorFullScreenPS(VaryingsFullScreen input) : SV_TARGET0
{
    // 从深度图重构当前像素的世界位置
    float depth = gDepthTex.Sample(gPointClampSampler, input.uv);
    float clearDepth = (gObjIndexOrReverseZFlag != 0) ? 0.0f : 1.0f;
    const float eps = 1e-4f;
    clip(abs(depth - clearDepth) - eps);
    float4 posWS = GetWorldPosition(input.uv, gCurrMatrix, depth);

    // 将当前世界位置变换到上一帧的裁剪空间
    float4 prevPosCS = mul(posWS, gPrevViewProj);
    // 透视除法得到 NDC 坐标
    prevPosCS /= prevPosCS.w;

    float2 prevUV = prevPosCS.xy * 0.5f + 0.5f;
    prevUV.y = 1.0f - prevUV.y;

    return input.uv - prevUV;
}


Varyings MotionVectorVS(Attributes input)
{
    Varyings output;

    ShaderResource::MeshData meshData = gMeshBuffer[gObjIndexOrReverseZFlag];
    ShaderResource::MeshData prevMeshData = gLastFrameMeshBuffer[gLastFrameObjIndex];
    float4 posWS = mul(float4(input.posOS, 1.0f), meshData.world);
    float4 prevPosWS = mul(float4(input.posOS, 1.0f), prevMeshData.world);
    output.currPosCS = mul(posWS, gCurrMatrix);
    output.currPos = output.currPosCS.xyw;
    output.prevPos = mul(prevPosWS, gPrevViewProj).xyw;

    return output;
}

float2 MotionVectorPS(Varyings input) : SV_TARGET0
{
    // 手动进行透视除法得到 NDC 坐标
    float3 currPosNDC = input.currPos / input.currPos.z;
    float3 prevPosNDC = input.prevPos / input.prevPos.z;
    float2 currUV = currPosNDC.xy * 0.5f + 0.5f;
    float2 prevUV = prevPosNDC.xy * 0.5f + 0.5f;
    currUV.y = 1.0f - currUV.y;
    prevUV.y = 1.0f - prevUV.y;

    return currUV - prevUV;
}
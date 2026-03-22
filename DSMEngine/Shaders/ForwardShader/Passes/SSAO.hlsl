#include "../Common.hlsli"
#include "../ResourceData.h"

#define MAX_SAMPLE_COUNT 14
#define THREAD_SIZE 16

RWTexture2D<float> gSSAOOutput : register(u0);
Texture2D<float2> gNormalTex : register(t0);
Texture2D<float> gDepthTex : register(t1);
Texture2D<float4> gNoiseTex : register(t2);

ConstantBuffer<ShaderResource::SSAOConstants> gSSAOConstants : register(b0);

// 四面体的 8 个顶点和 6 个面的中点
static float3 sSamplePoints[MAX_SAMPLE_COUNT] = {
    {1, 1, 1},
    {-1, -1, -1},
    {-1, 1, 1},
    {1, -1, -1},
    {1, 1, -1},
    {-1, -1, 1},
    {-1, 1, -1},
    {1, -1, 1},
    {-1, 0, 0},
    {1, 0, 0},
    {0, -1, 0},
    {0, 1, 0},
    {0, 0, -1},
    {0, 0, 1}
};

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void SSAOCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int width, height;
    gSSAOOutput.GetDimensions(width, height);
    if(dispatchThreadID.x >= width || dispatchThreadID.y >= height){
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) / float2(width, height);
    float3 normal = DecodeFloat2ToFloat3(gNormalTex.Sample(gLinearBorderSampler, uv));

    // 获取随机向量
    float4 noise = gNoiseTex.Sample(gPointWrapSampler, uv);
    noise.xyz = normalize(noise.xyz * 2 - 1);

    // 获取视图空间的坐标
    float depth = gDepthTex.Sample(gLinearBorderSampler, uv);
    float4 posCS = float4(uv * 2 - 1, depth, 1);
    posCS.y *= -1;  // 需要翻转
    float4 posVS = mul(posCS, gSSAOConstants.projInv);
    posVS /= posVS.w;

    float occlusion = 0;
    int sampleCount = clamp(gSSAOConstants.sampleCount, 0, MAX_SAMPLE_COUNT);
    for(int i = 0; i < sampleCount; ++i){
        // 将均匀分布的采样偏移进行随机偏移
        float3 sampleOffset = reflect(sSamplePoints[i] * noise.w, noise.xyz);
        // 将采样偏移朝向法线半球方向
        sampleOffset *= sign(dot(normal, sampleOffset));
        float3 samplePoint = posVS.xyz + sampleOffset * gSSAOConstants.occlusionRadius;

        // 获取采样点方向上的深度
        float4 samplePointCS = mul(float4(samplePoint, 1), gSSAOConstants.proj);
        samplePointCS /= samplePointCS.w;
        float2 sampleUV = samplePointCS.xy * 0.5 + 0.5;
        sampleUV.y = 1 - sampleUV.y;   // 纹理坐标反转
        float sampleDepth = gDepthTex.Sample(gLinearBorderSampler, sampleUV);
        sampleDepth = GetLinearDepth(sampleDepth, gSSAOConstants.proj);

        // 沿视线方向最近的点
        float3 pointInLine = (sampleDepth / samplePoint.z) * samplePoint;
        float3 dir = pointInLine - posVS.xyz;
        float distance = -dir.z; // 遮挡点与视点的距离
        float angle = max(0, dot(normalize(dir), normal)); // 遮挡点与法线的夹角

        // 限制阈值
        if(distance > gSSAOConstants.ssaoThreshold){
            float fadeEnd = gSSAOConstants.fadeEnd;
            float fade = (fadeEnd - distance) / fadeEnd - gSSAOConstants.ssaoThreshold;
            occlusion += angle * saturate(fade);
        }
    }
    occlusion /= sampleCount;

    // 提高对比度
    gSSAOOutput[dispatchThreadID.xy] = pow(1 - saturate(occlusion), gSSAOConstants.contrast);
}
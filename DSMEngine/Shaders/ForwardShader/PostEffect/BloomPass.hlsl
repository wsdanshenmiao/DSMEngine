#ifndef __BLOOMPASS_HLSL__
#define __BLOOMPASS_HLSL__

#include "../Common.hlsli"

#define THREAD_SIZE 8

cbuffer BloomConstants : register(b0)
{
    float2 gInvSrcSize;
    float2 gInvDstSize;

    float gThreshold;
    float gKnee;
    float gIntensity;
    float gScatter;

    float gClampMax;
    float3 gPadding;
}

RWTexture2D<float4> gDstTex : register(u0);
Texture2D<float4> gSrcTex : register(t0);
Texture2D<float4> gAddTex : register(t1);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 ClampHdr(float3 c)
{
    return min(c, gClampMax.xxx);
}

float3 SampleSrc(float2 uv)
{
    return gSrcTex.SampleLevel(gLinearClampSampler, uv, 0.0f).rgb;
}

float3 Downsample13Tap(float2 uv)
{
    float2 o = gInvSrcSize;

    float3 a = SampleSrc(uv + float2(-2.0f, -2.0f) * o);
    float3 b = SampleSrc(uv + float2(0.0f, -2.0f) * o);
    float3 c = SampleSrc(uv + float2(2.0f, -2.0f) * o);

    float3 d = SampleSrc(uv + float2(-2.0f, 0.0f) * o);
    float3 e = SampleSrc(uv);
    float3 f = SampleSrc(uv + float2(2.0f, 0.0f) * o);

    float3 g = SampleSrc(uv + float2(-2.0f, 2.0f) * o);
    float3 h = SampleSrc(uv + float2(0.0f, 2.0f) * o);
    float3 i = SampleSrc(uv + float2(2.0f, 2.0f) * o);

    float3 j = SampleSrc(uv + float2(-1.0f, -1.0f) * o);
    float3 k = SampleSrc(uv + float2(1.0f, -1.0f) * o);
    float3 l = SampleSrc(uv + float2(-1.0f, 1.0f) * o);
    float3 m = SampleSrc(uv + float2(1.0f, 1.0f) * o);

    float3 cross = (b + d + f + h) * (1.0f / 8.0f);
    float3 corners = (a + c + g + i) * (1.0f / 16.0f);
    float3 inner = (j + k + l + m) * (1.0f / 8.0f);
    float3 center = e * (1.0f / 4.0f);

    return corners + cross + inner + center;
}

float3 UpsampleTent(float2 uv)
{
    float2 o = gInvSrcSize;

    float3 c00 = SampleSrc(uv + float2(-1.0f, -1.0f) * o);
    float3 c10 = SampleSrc(uv + float2(0.0f, -1.0f) * o);
    float3 c20 = SampleSrc(uv + float2(1.0f, -1.0f) * o);

    float3 c01 = SampleSrc(uv + float2(-1.0f, 0.0f) * o);
    float3 c11 = SampleSrc(uv);
    float3 c21 = SampleSrc(uv + float2(1.0f, 0.0f) * o);

    float3 c02 = SampleSrc(uv + float2(-1.0f, 1.0f) * o);
    float3 c12 = SampleSrc(uv + float2(0.0f, 1.0f) * o);
    float3 c22 = SampleSrc(uv + float2(1.0f, 1.0f) * o);

    float3 row0 = c00 + c20 + 2.0f * c10;
    float3 row1 = c01 + c21 + 2.0f * c11;
    float3 row2 = c02 + c22 + 2.0f * c12;

    return (row0 + row2 + 2.0f * row1) * (1.0f / 16.0f);
}

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void BloomPrefilterCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    gDstTex.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) {
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) / float2(width, height);
    float3 hdr = ClampHdr(SampleSrc(uv));
    float brightness = Luminance(hdr);
    float knee = max(gKnee, 1e-5f);

    float soft = brightness - gThreshold + knee;
    soft = saturate(soft / (2.0f * knee));
    soft = soft * soft * knee * 0.25f;
    float hard = max(brightness - gThreshold, 0.0f);
    float contrib = max(hard, soft) / max(brightness, 1e-5f);
    contrib = saturate(contrib);

    gDstTex[dispatchThreadID.xy] = float4(hdr * contrib, 1.0f);
}

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void BloomDownsampleCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    gDstTex.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) {
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) / float2(width, height);
    float3 c = ClampHdr(Downsample13Tap(uv));
    gDstTex[dispatchThreadID.xy] = float4(c, 1.0f);
}

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void BloomUpsampleCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    gDstTex.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) {
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) / float2(width, height);
    float3 low = UpsampleTent(uv);
    float3 high = gAddTex.SampleLevel(gLinearClampSampler, uv, 0.0f).rgb;
    float scatter = saturate(gScatter);
    float3 outColor = high + low * scatter;

    gDstTex[dispatchThreadID.xy] = float4(ClampHdr(outColor), 1.0f);
}

[numthreads(THREAD_SIZE, THREAD_SIZE, 1)]
void BloomCompositeCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    gDstTex.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) {
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) / float2(width, height);
    float3 scene = gSrcTex.SampleLevel(gLinearClampSampler, uv, 0.0f).rgb;
    float3 bloom = gAddTex.SampleLevel(gLinearClampSampler, uv, 0.0f).rgb;
    gDstTex[dispatchThreadID.xy] = float4(scene + bloom * gIntensity, 1.0f);
}

#endif // __BLOOMPASS_HLSL__

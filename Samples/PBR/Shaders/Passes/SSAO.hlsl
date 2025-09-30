#include "Common.hlsl"

RWTexture2D<float> gSSAOOutput : register(u0);
Texture2D<float2> gNormalTex : register(t0);
Texture2D<float> gDepthTex : register(t1);
Texture2D<float4> gNoiseTex : register(t2);


[numthreads(1,1,1)]
void SSAOCS(uint3 dispatchThreadID : DispatchThreadID)
{
    float3 normal = DecodeNormal(gNormalTex[dispatchThreadID.xy]);
    float depth = gDepthTex[dispatchThreadID.xy];
    float4 noise = gNoiseTex[dispatchThreadID.xy];

    gSSAOOutput[dispatchThreadID.xy] = ComputeSSAO(normal, depth, noise);
}
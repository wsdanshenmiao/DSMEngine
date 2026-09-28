#include "RestirDICommon.hlsli"

struct PresentVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

PresentVertexOutput PresentVS(uint vertexID : SV_VertexID)
{
    PresentVertexOutput output;
    output.uv = float2((vertexID << 1u) & 2u, vertexID & 2u);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float3 ACESFitted(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / max(color * (c * color + d) + e, 1e-6f));
}

float4 PresentPS(PresentVertexOutput input) : SV_Target0
{
    uint2 pixel = min(uint2(input.position.xy), g_Frame.resolutionFrame.xy - 1u);
    uint index = pixel.y * g_Frame.resolutionFrame.x + pixel.x;
    float3 hdr = max(g_HdrInput[index].rgb, 0.0f.xxx) * exp2(g_Frame.cameraExposure.w);
    return float4(ACESFitted(hdr), 1.0f);
}

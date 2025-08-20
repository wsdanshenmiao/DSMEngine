#ifndef __COLOR_H__
#define __COLOR_H__

struct VertexPosLColor
{
    float3 PosLS : POSITION;
    float4 Color : COLOR;
};

struct VertexPosHColor
{
    float4 PosCS : SV_Position;
    float4 Color : COLOR;
};

struct ObjectConstants
{
    float4x4 World;
    float4x4 WorldInvTranspose;
};

struct PassConstants
{
    float4x4 View;
    float4x4 InvView;
    float4x4 Proj;
    float4x4 InvProj;
    float3 EyePosW;
    float pad;
};

ConstantBuffer<ObjectConstants> g_ObjectCB : register(b0);
ConstantBuffer<PassConstants> g_PassCB : register(b1);

VertexPosHColor VS(VertexPosLColor i)
{
    VertexPosHColor o;
    // float4x4 viewProj = mul(g_PassCB.View, g_PassCB.Proj);
    // float4 posWS = mul(float4(i.PosLS, 1), g_ObjectCB.World);
    // o.PosCS = mul(posWS, viewProj);
    o.PosCS = float4(i.PosLS, 1);
    o.Color = i.Color;
    return o;
}

float4 PS(VertexPosHColor i) : SV_Target
{
    return i.Color;
}

#endif
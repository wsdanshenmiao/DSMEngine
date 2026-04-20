#ifndef __CONSTANTBUFFERS_HLSLI__
#define __CONSTANTBUFFERS_HLSLI__


#if defined(__cplusplus)
using float2 = DSM::Math::Vector2;
using float3 = DSM::Math::Vector3;
using float4 = DSM::Math::Vector4;
using float3x3 = DSM::Math::Matrix3;
using float4x4 = DSM::Math::Matrix4;
using uint = uint32_t;
#endif

#define MAX_DIRECTIONAL_LIGHT_COUNT 4
#define MAX_SHADOWED_DIRECTIONAL_LIGHT_COUNT 4
#define MAX_CASCADES_PER_LIGHT 4

namespace ShaderResource{

    enum MaterialTex
    {
        kBaseColor, kDiffuseRoughness, kMetalness, kOcclusion, kEmissive, kNormal, kNumTextures
    };

    struct MaterialData
    {
        float4 baseColor;
        float4 emissiveColor;
        float normalTexScale;
        float metallicFactor;
        float roughnessFactor;
        int textureIndex[MaterialTex::kNumTextures];
        float pad[3];
    };

    struct MeshData
    {
        float4x4 world;
        float4x4 worldIT;
    };

    struct PassConstants
    {
        float4x4 view;
        float4x4 viewInv;
        float4x4 proj;
        float4x4 projInv;
        float4 renderTargetSize;
        float4 nearFarZ;
        float3 cameraPos;
        float deltaTime;
    };

    struct SkyboxConstants
    {
        float4x4 invViewProj;
        float4 cameraPos;
        bool isReversedZ;
    };

    struct SSAOConstants
    {
        float4x4 proj;
        float4x4 projInv;
        float sampleCount;
        float occlusionRadius;
        float ssaoThreshold;
        float fadeEnd;
        uint contrast;
        float pad[3];
    };

    struct SSAOBlurConstants
    {
        float4x4 proj;
        float blurRadius;
        bool isHorizontal;
        float pad[2];
    };

    struct LightData
    {
        uint dirLightCount;
        uint otherLightCount;
    };

    struct DirectionalLightData
    {
        float4 color;
        float4 direction;
        float4 shadowData;
    };

    struct OtherLightData
    {
        float4 color;
        float4 direction;
        float4 positionAndRange;
        float4 spotAngle;   // inner and outer angle
        float4 shadowData;
    };

    struct ShadowConstants
    {
        // 级联到远平面的距离
        float4 cascadeFarPlaneDist;
        
        float recMaxDistance;
        float recDistanceFade;
        float cascadeFade;

        uint cascadeCount;
    };
    
}

#endif
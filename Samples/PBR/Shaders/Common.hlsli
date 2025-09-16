#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

// 默认采样器
SamplerState gDefaultSampler : register(s0);
// 比较采样器，用于阴影贴图采样
SamplerComparisonState gShadowSampler : register(s1);


#endif // __COMMON_HLSLI__
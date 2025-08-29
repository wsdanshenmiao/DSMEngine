#pragma once
#ifndef __MATERIAL_H__
#define __MATERIAL_H__

#include <array>


namespace DSM {
    enum MaterialTex
    {
        kBaseColor, kDiffuseRoughness, kMetalness, kOcclusion, kEmissive, kNormal, kNumTextures
    };

    __declspec(align(256)) struct Material
    {
        float baseColor[4] = {1,1,1,1};
        float emissiveColor[3] = {0,0,0};
        float normalTexScale = 1;
        float metallicFactor = 1;
        float roughnessFactor = 1;
    };
}

#endif

#pragma once
#ifndef __MATERIAL_H__
#define __MATERIAL_H__

#include <array>
#include "Runtime/Math/MathCommon.h"

namespace DSM {
    enum MaterialTex
    {
        kBaseColor, kDiffuseRoughness, kMetalness, kOcclusion, kEmissive, kNormal, kNumTextures
    };

    struct Material
    {
        Math::Vector4 baseColor = {1,1,1,1};
        Math::Vector3 emissiveColor = {0,0,0};
        float normalTexScale = 1;
        float metallicFactor = 1;
        float roughnessFactor = 1;
    };
}

#endif

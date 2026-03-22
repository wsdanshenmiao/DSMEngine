#pragma once
#ifndef __LIGHT_H__
#define __LIGHT_H__

#include "Runtime/Math/Transform.h"

namespace DSM{
    enum class LightType
    {
        Directional,
        Point,
        Spot
    };

    struct Light
    {
        LightType lightType;
        Math::Vector4 color;
        Math::Vector3 direction;
        Math::Vector3 position;
        float range;
        float innerAngle;
        float outerAngle;

        constexpr Light& SetType(LightType type) { lightType = type; return *this; }
        constexpr Light& SetColor(const Math::Vector4& c) { color = c; return *this; }
        constexpr Light& SetDirection(const Math::Vector3& dir) { direction = dir; return *this; }
        constexpr Light& SetPosition(const Math::Vector3& pos) { position = pos; return *this; }
        constexpr Light& SetRange(float r) { range = r; return *this; }
        constexpr Light& SetInnerAngle(float angle) { innerAngle = angle; return *this; }
        constexpr Light& SetOuterAngle(float angle) { outerAngle = angle; return *this; }
    };
    

}


#endif
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
    };
    

}


#endif
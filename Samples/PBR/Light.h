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
        float range;
        float spotAngle;
        Math::Transform transform;
    };
    

}


#endif
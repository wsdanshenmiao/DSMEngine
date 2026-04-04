#pragma once
#ifndef __LIGHTCOMPONENT_H__
#define __LIGHTCOMPONENT_H__

#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Math/MathCommon.h"

namespace DSM {
    enum class LightType
    {
        Directional,
        Point,
        Spot
    };

    class Light : public IComponent
    {
    public:
        Light(std::shared_ptr<GameObject> gameObject)
            : IComponent(gameObject) {}

        LightType GetType() const noexcept { return lightType; }
        const Math::Vector4& GetColor() const noexcept { return color; }
        const Math::Vector3& GetDirection() const noexcept { return direction; }
        const Math::Vector3& GetPosition() const noexcept { return position; }
        float GetRange() const noexcept { return range; }
        float GetInnerAngle() const noexcept { return innerAngle; }
        float GetOuterAngle() const noexcept { return outerAngle; }

        constexpr Light& SetType(LightType type) { lightType = type; return *this; }
        constexpr Light& SetColor(const Math::Vector4& c) { color = c; return *this; }
        constexpr Light& SetDirection(const Math::Vector3& dir) { direction = dir; return *this; }
        constexpr Light& SetPosition(const Math::Vector3& pos) { position = pos; return *this; }
        constexpr Light& SetRange(float r) { range = r; return *this; }
        constexpr Light& SetInnerAngle(float angle) { innerAngle = angle; return *this; }
        constexpr Light& SetOuterAngle(float angle) { outerAngle = angle; return *this; }
        
    private:
        LightType lightType{};
        Math::Vector4 color{};
        Math::Vector3 direction{};
        Math::Vector3 position{};
        float range{};
        float innerAngle{};
        float outerAngle{};
    };
} // namespace DSM



#endif
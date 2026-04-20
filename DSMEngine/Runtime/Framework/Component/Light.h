#pragma once
#ifndef __LIGHTCOMPONENT_H__
#define __LIGHTCOMPONENT_H__

#include <cmath>
#include <limits>
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Math/Collision/BoundingBox.h"
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
        Math::Vector3 GetDirection() const noexcept
        {
            if(auto obj = m_GameObject.lock(); obj != nullptr) {
                if(auto transform = obj->GetComponent<TransformComponent>(); transform != nullptr){
                    return transform->GetForwardAxis();
                }
            }
            return Math::Vector3{0, 0, -1};
        }
        Math::Vector3 GetPosition() const noexcept
        {
            if(auto obj = m_GameObject.lock(); obj != nullptr) {
                if(auto transform = obj->GetComponent<TransformComponent>(); transform != nullptr){
                    return transform->GetPosition();
                }
            }
            return Math::Vector3{0, 0, 0};
        }
        float GetRange() const noexcept { return range; }
        float GetInnerAngle() const noexcept { return innerAngle; }
        float GetOuterAngle() const noexcept { return outerAngle; }
        Math::AxisAlignedBox GetBounds() const noexcept
        {
            constexpr float kEpsilon = 1e-6f;
            constexpr float kDirectionalExtent = 1e6f;

            const Math::Vector3 position = GetPosition();
            const float safeRange = std::max(0.0f, range);

            switch (lightType) {
            case LightType::Directional: {
                Math::Vector3 extents{kDirectionalExtent, kDirectionalExtent, kDirectionalExtent};
                return Math::AxisAlignedBox(position - extents, position + extents);
            }
            case LightType::Point: {
                Math::Vector3 extents{safeRange, safeRange, safeRange};
                return Math::AxisAlignedBox(position - extents, position + extents);
            }
            case LightType::Spot: {
                if (safeRange <= kEpsilon) {
                    return Math::AxisAlignedBox{};
                }

                const Math::Vector3 lightDir = GetDirection().NearZero()
                    ? Math::Vector3{0, 0, -1}
                    : GetDirection().Normalized();

                // For very wide cones tan(theta) approaches infinity. Use a sphere fallback.
                const float maxHalfAngle = std::numbers::pi_v<float> * 0.5f - 1e-4f;
                const float halfAngle = std::clamp(outerAngle, 0.0f, maxHalfAngle);
                const float radius = safeRange * std::tan(halfAngle);

                const Math::Vector3 baseCenter = position + lightDir * safeRange;

                auto axisExtent = [&](int axis) {
                    const float dirAxis = lightDir.Get(axis);
                    const float radial = radius * std::sqrt(std::max(0.0f, 1.0f - dirAxis * dirAxis));
                    const float apexVal = position.Get(axis);
                    const float baseVal = baseCenter.Get(axis);
                    const float minVal = std::min(apexVal, baseVal - radial);
                    const float maxVal = std::max(apexVal, baseVal + radial);
                    return Math::Vector2{minVal, maxVal};
                };

                const Math::Vector2 xBounds = axisExtent(0);
                const Math::Vector2 yBounds = axisExtent(1);
                const Math::Vector2 zBounds = axisExtent(2);
                Math::Vector3 minPoint{xBounds.Get(0), yBounds.Get(0), zBounds.Get(0)};
                Math::Vector3 maxPoint{xBounds.Get(1), yBounds.Get(1), zBounds.Get(1)};
                return Math::AxisAlignedBox(minPoint, maxPoint);
            }
            default:
                break;
            }

            return Math::AxisAlignedBox{};
        }

        constexpr Light& SetType(LightType type) { lightType = type, m_IsDirty = true; return *this; }
        constexpr Light& SetRange(float r) { range = r; m_IsDirty = true; return *this; }
        constexpr Light& SetInnerAngle(float angle) { innerAngle = angle; m_IsDirty = true; return *this; }
        constexpr Light& SetOuterAngle(float angle) { outerAngle = angle; m_IsDirty = true; return *this; }
        constexpr Light& SetColor(const Math::Vector4& c) { color = c; m_IsDirty = true; return *this; }

        Light& SetDirection(const Math::Vector3& dir)
        { 
            if(dir.NearZero())
                return *this;

            auto direction = dir.Normalized();
            if(auto obj = m_GameObject.lock(); obj != nullptr) {
                if(auto transform = obj->GetComponent<TransformComponent>(); transform != nullptr) {
                    Math::Vector3 up{0, 1, 0};
                    if (std::abs(Math::Vector3::Dot(direction, up)) > 0.999f) {
                        up = Math::Vector3{0, 0, 1};
                    }
                    transform->LookTo(direction, up);
                }
            }

            m_IsDirty = true;
            return *this; 
        }

        Light& SetDirection(const Math::Quaternion& rot)
        {
            if(m_GameObject.expired()) {
                return *this;
            }
            auto transform = m_GameObject.lock()->GetComponent<TransformComponent>();
            if(transform != nullptr){
                transform->SetRotation(rot);
            }
            m_IsDirty = true;
            return *this;
        }
        
        Light& SetPosition(const Math::Vector3& pos)
        { 
            if(auto obj = m_GameObject.lock(); obj != nullptr) {
                if(auto transform = obj->GetComponent<TransformComponent>(); transform != nullptr){
                    transform->SetPosition(pos);
                }
            }
            m_IsDirty = true;
            return *this;
        }
        
    private:
        LightType lightType = LightType::Directional;
        Math::Vector4 color{ 1, 1, 1, 1 };
        float range{};
        float innerAngle{};
        float outerAngle{};
    };
} // namespace DSM



#endif
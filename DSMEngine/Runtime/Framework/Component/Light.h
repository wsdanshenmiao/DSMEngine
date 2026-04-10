#pragma once
#ifndef __LIGHTCOMPONENT_H__
#define __LIGHTCOMPONENT_H__

#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Framework/Object/GameObject.h"
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
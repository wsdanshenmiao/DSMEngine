#pragma once
#ifndef __TRANSFORM_DRAWER_H__
#define __TRANSFORM_DRAWER_H__


#include "ComponentDrawer.h"


namespace DSM {
    struct TransformDrawer : public IComponentDrawer
    {
        bool CanDraw(const std::shared_ptr<GameObject>& object) override
        {
            return object->HasComponent<TransformComponent>();
        }
        inline const char* GetName() override { return "Transform"; }

        void DrawUI(const std::shared_ptr<GameObject>& object) override
        {
            assert(object != nullptr);

            const auto& transform = object->GetComponent<TransformComponent>();
            Math::Vector3 pos = transform->GetPosition();
            Math::Vector3 scale = transform->GetScale();
            Math::Vector3 degree = Math::RadiansToDegree(transform->GetRotation().ToEulerAngles());
            DrawVec3Control("Position", pos);
            DrawVec3Control("Rotation", degree);
            DrawVec3Control("Scale", scale, 1);
            transform->SetPosition(pos);
            transform->SetRotation(Math::Quaternion{Math::DegreeToRadians(degree)});
            transform->SetScale(scale);
        }
        void AddComponent(const std::shared_ptr<GameObject>& object) override
        {
            assert(object != nullptr);

            if (!object->HasComponent<TransformComponent>()) {
                object->AddComponent<TransformComponent>();
            }
        }
        void RemoveComponent(const std::shared_ptr<GameObject>& object)
        {
            assert(object != nullptr);

            if (object->HasComponent<TransformComponent>()) {
                object->RemoveComponent<TransformComponent>();
            }
        }
    };
}





#endif // __TRANSFORM_DRAWER_H__
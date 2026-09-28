#pragma once
#ifndef __CAMERA_DRAWER_H__
#define __CAMERA_DRAWER_H__

#include "ComponentDrawer.h"
#include "Runtime/Framework/Component/CameraComponent.h"


namespace DSM{
    struct CameraDrawer : public IComponentDrawer
    {
        bool CanDraw(const std::shared_ptr<GameObject>& object) override
        {
            return object->HasComponent<CameraComponent>();
        }
        const char* GetName() override { return "Camera"; }
        void DrawUI(const std::shared_ptr<GameObject>& object) override
        {
            DSM_CORE_ASSERT(object != nullptr);
            DSM_CORE_ASSERT(object->HasComponent<CameraComponent>());

            auto& camera = *object->GetComponent<CameraComponent>();
            ImGui::PushItemWidth(-FLT_MIN);

            float cameraFov = Math::RadiansToDegree(camera.GetFovY());
            float editedFov = DrawFloatControl("Vertical Fov", cameraFov, 1.0f, 0.0f, 360.0f);
            if (editedFov != cameraFov) {
                camera.SetFovY(Math::DegreeToRadians(editedFov));
            }

            float nearZ = camera.GetNearZ();
            float editedNearZ = DrawFloatControl("Near Plane", nearZ, 1.0f, 0.001f, std::numeric_limits<float>::max());
            if (editedNearZ != nearZ) {
                camera.SetNearZ(editedNearZ);
            }

            float farZ = camera.GetFarZ();
            float editedFarZ = DrawFloatControl("Far Plane", farZ, 1.0f, editedNearZ, std::numeric_limits<float>::max());
            if (editedFarZ != farZ) {
                camera.SetFarZ(std::max(editedNearZ, editedFarZ));
            }

            bool reverse = camera.IsReversedZ();
            bool editedReverse = DrawBoolControl("Reverse Z", reverse);
            if (editedReverse != reverse) {
                camera.ReverseZ(editedReverse);
            }
            ImGui::PopItemWidth();
        }
        void AddComponent(const std::shared_ptr<GameObject>& object) override
        {
            assert(object != nullptr);
            if (!object->HasComponent<CameraComponent>()) {
                object->AddComponent<CameraComponent>();
            }
        }
        void RemoveComponent(const std::shared_ptr<GameObject>& object)
        {
            assert(object != nullptr);
            if (object->HasComponent<CameraComponent>()) {
                object->RemoveComponent<CameraComponent>();
            }
        }
    };
}



#endif // __CAMERA_DRAWER_H__
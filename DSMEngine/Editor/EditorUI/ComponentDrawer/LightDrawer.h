#pragma once
#ifndef __LIGHT_DRAWER_H__
#define __LIGHT_DRAWER_H__

#include "ComponentDrawer.h"
#include "Runtime/Framework/Component/Light.h"

namespace DSM {
    struct LightDrawer : public IComponentDrawer
    {
        bool CanDraw(const std::shared_ptr<GameObject>& object) override
        {
            return object->HasComponent<Light>();
        }

        const char* GetName() override { return "Light"; }

        void DrawUI(const std::shared_ptr<GameObject>& object) override
        {
            DSM_CORE_ASSERT(object != nullptr);
            DSM_CORE_ASSERT(object->HasComponent<Light>());

            auto& light = *object->GetComponent<Light>();
            ImGui::PushItemWidth(-FLT_MIN);

            int type = int(light.GetType());
            const char* items[] = { "Directional", "Point", "Spot" };
            DrawPropertyRow("Type", [&]() {
                if (ImGui::Combo("##Value", &type, items, IM_ARRAYSIZE(items))) {
                    light.SetType(LightType(type));
                }
            });

            Math::Vector4 color = light.GetColor();
            float colorVal[4] = {
                float(color.Get(0)),
                float(color.Get(1)),
                float(color.Get(2)),
                float(color.Get(3))
            };
            DrawPropertyRow("Color", [&]() {
                if (DrawColorEditorRight(colorVal)) {
                    color.Set(0, colorVal[0]);
                    color.Set(1, colorVal[1]);
                    color.Set(2, colorVal[2]);
                    color.Set(3, colorVal[3]);
                    light.SetColor(color);
                }
            });

            Math::Vector3 direction = light.GetDirection();
            DrawVec3Control("Direction", direction);
            light.SetDirection(direction);

            Math::Vector3 position = light.GetPosition();
            DrawVec3Control("Position", position);
            light.SetPosition(position);

            float range = light.GetRange();
            float editedRange = DrawFloatControl("Range", range, 0.1f, 0.0f, std::numeric_limits<float>::max());
            if (editedRange != range) {
                light.SetRange(std::max(0.0f, editedRange));
            }

            float halfPi = std::numbers::pi_v<float> / 2.2f;
            float innerAngle = light.GetInnerAngle();
            float editedInnerAngle = DrawSliderFloatControl("Inner Angle", innerAngle, 0.0f, halfPi);
            if (editedInnerAngle != innerAngle) {
                light.SetInnerAngle(editedInnerAngle);
            }

            float outerAngle = light.GetOuterAngle();
            float editedOuterAngle = DrawSliderFloatControl("Outer Angle", outerAngle, editedInnerAngle, halfPi);
            if (editedOuterAngle != outerAngle) {
                light.SetOuterAngle(std::max(editedInnerAngle, editedOuterAngle));
            }
            ImGui::PopItemWidth();
        }

        void AddComponent(const std::shared_ptr<GameObject>& object) override
        {
            assert(object != nullptr);
            if (!object->HasComponent<Light>()) {
                object->AddComponent<Light>();
            }
        }

        void RemoveComponent(const std::shared_ptr<GameObject>& object)
        {
            assert(object != nullptr);
            if (object->HasComponent<Light>()) {
                object->RemoveComponent<Light>();
            }
        }
    };
}

#endif // __LIGHT_DRAWER_H__
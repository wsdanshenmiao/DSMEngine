#pragma once
#ifndef __NATIVE_SCRIPT_DRAWER_H__
#define __NATIVE_SCRIPT_DRAWER_H__

#include "ComponentDrawer.h"
#include "Runtime/Framework/Component/NativeScript.h"

namespace DSM {
    struct NativeScriptDrawer : public IComponentDrawer
    {
        bool CanDraw(const std::shared_ptr<GameObject>& object) override
        {
            return object->HasComponent<NativeScript>();
        }

        const char* GetName() override { return "Native Script"; }

        bool HasEnableToggle(const std::shared_ptr<GameObject>& object) override
        {
            return object != nullptr && object->HasComponent<NativeScript>();
        }

        bool GetEnabled(const std::shared_ptr<GameObject>& object) override
        {
            if (object == nullptr) return true;
            auto script = object->GetComponent<NativeScript>();
            return script == nullptr ? true : script->IsEnabled();
        }

        void SetEnabled(const std::shared_ptr<GameObject>& object, bool enabled) override
        {
            if (object == nullptr) return;
            if (auto script = object->GetComponent<NativeScript>(); script != nullptr) {
                script->SetEnabled(enabled);
            }
        }

        void DrawUI(const std::shared_ptr<GameObject>& object) override
        {
            DSM_CORE_ASSERT(object != nullptr);
            DSM_CORE_ASSERT(object->HasComponent<NativeScript>());

            auto& script = *object->GetComponent<NativeScript>();

            ImGui::Separator();
            ImGui::Text("Script");
            ImGui::TextDisabled("Type: %s", script.GetScript() == nullptr ? "None" : "Bound");
        }

        void AddComponent(const std::shared_ptr<GameObject>& object) override
        {
            assert(object != nullptr);
            if (!object->HasComponent<NativeScript>()) {
                object->AddComponent<NativeScript>();
            }
        }

        void RemoveComponent(const std::shared_ptr<GameObject>& object)
        {
            assert(object != nullptr);
            if (object->HasComponent<NativeScript>()) {
                object->RemoveComponent<NativeScript>();
            }
        }
    };
}

#endif // __NATIVE_SCRIPT_DRAWER_H__
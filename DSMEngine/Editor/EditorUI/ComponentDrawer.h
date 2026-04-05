#pragma once
#ifndef __COMPONENT_DRAWER_H__
#define __COMPONENT_DRAWER_H__

#include <imgui_internal.h>

#include "Runtime/Math/MathCommon.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Framework/Component/CameraComponent.h"
#include "Runtime/Framework/Component/Light.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Framework/Component/NativeScript.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Core/Macro.h"


namespace DSM{
    template <typename DrawFunc>
    inline void DrawPropertyRow(const char* label, DrawFunc&& drawControl, float columnWidth = 120.0f)
    {
        ImGui::PushID(label);
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::TextUnformatted(label);
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        drawControl();
        ImGui::Columns(1);
        ImGui::PopID();
    }

    inline bool DrawColorEditorRight(float color[4], float width = 170.0f)
    {
        float availWidth = ImGui::GetContentRegionAvail().x;
        float barWidth = (std::min)(width, availWidth);
        float barHeight = ImGui::GetFrameHeight();

        if (availWidth > barWidth) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - barWidth));
        }

        bool changed = false;
        ImVec4 col{color[0], color[1], color[2], color[3]};
        if (ImGui::ColorButton("##ColorBar", col, ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2{barWidth, barHeight})) {
            ImGui::OpenPopup("##ColorPickerPopup");
        }

        if (ImGui::BeginPopup("##ColorPickerPopup")) {
            changed |= ImGui::ColorPicker4("##ColorPicker", color);
            ImGui::EndPopup();
        }

        return changed;
    }

    inline void DrawVec3Control(const std::string& label, Math::Vector3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
    {
        ImGuiIO& io = ImGui::GetIO();
        const auto& boldFont = io.Fonts->Fonts[0];

        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

        auto showVal = [&boldFont, buttonSize, resetValue](
            const char* name, float& val,
            const ImVec4& col0, const ImVec4& col1, const ImVec4& col2) {
            ImGui::PushStyleColor(ImGuiCol_Button, col0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col1);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, col2);
            ImGui::PushFont(boldFont);
            if (ImGui::Button(name, buttonSize)) {
                val = resetValue;
            }
            ImGui::PopFont();
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            std::string valLabel = "##" + std::string(name);
            ImGui::DragFloat(valLabel.c_str(), &val, 0.1f, 0.0f, 0.0f, "%.2f");
            ImGui::PopItemWidth();
        };

        float val = values.Get(0);
        showVal("X", val, ImVec4{0.8f, 0.1f, 0.15f, 1.0f}, ImVec4{0.9f, 0.2f, 0.2f, 1.0f}, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
        values.Set(0, val);
        ImGui::SameLine();

        val = values.Get(1);
        showVal("Y", val, ImVec4{0.2f, 0.7f, 0.2f, 1.0f}, ImVec4{0.3f, 0.8f, 0.3f, 1.0f}, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
        values.Set(1, val);
        ImGui::SameLine();

        val = values.Get(2);
        showVal("Z", val, ImVec4{0.1f, 0.25f, 0.8f, 1.0f}, ImVec4{0.2f, 0.35f, 0.9f, 1.0f}, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
        values.Set(2, val);

        ImGui::PopStyleVar();

        ImGui::Columns(1);

        ImGui::PopID();
    }

    inline void DrawMaterialUI(const std::shared_ptr<Material>& material)
    {
        if (material == nullptr) {
            ImGui::TextDisabled("Material: None");
            return;
        }

        ImGui::Separator();
        bool opened = ImGui::TreeNodeEx("##MaterialFoldout", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen, "Material");

        if (!opened) {
            return;
        }

        ImGui::Indent();

        ImGui::PushID("MaterialProps");
        Math::Vector4 color = material->GetBaseColor();
        float baseColor[4] = { color.Get(0), color.Get(1), color.Get(2), color.Get(3) };
        DrawPropertyRow("Base Color", [&]() {
            if (DrawColorEditorRight(baseColor)) {
                color.Set(0, baseColor[0]);
                color.Set(1, baseColor[1]);
                color.Set(2, baseColor[2]);
                color.Set(3, baseColor[3]);
                material->SetBaseColor(color);
            }
        });

        color = material->GetEmissiveColor();
        float emissiveColor[4] = { color.Get(0), color.Get(1), color.Get(2), color.Get(3) };
        DrawPropertyRow("Emissive Color", [&]() {
            if (DrawColorEditorRight(emissiveColor)) {
                color.Set(0, emissiveColor[0]);
                color.Set(1, emissiveColor[1]);
                color.Set(2, emissiveColor[2]);
                color.Set(3, emissiveColor[3]);
                material->SetEmissiveColor(color);
            }
        });

        float normalTexScale = material->GetNormalTexScale();
        DrawPropertyRow("Normal Scale", [&]() {
            if (ImGui::DragFloat("##Value", &normalTexScale, 0.01f, 0.0f, 16.0f, "%.2f")) {
                material->SetNormalTexScale(std::max(0.0f, normalTexScale));
            }
        });

        float metallic = material->GetMetallicFactor();
        DrawPropertyRow("Metallic", [&]() {
            if (ImGui::SliderFloat("##Value", &metallic, 0.0f, 1.0f)) {
                material->SetMetallicFactor(metallic);
            }
        });

        float roughness = material->GetRoughnessFactor();
        DrawPropertyRow("Roughness", [&]() {
            if (ImGui::SliderFloat("##Value", &roughness, 0.0f, 1.0f)) {
                material->SetRoughnessFactor(roughness);
            }
        });

        static const char* kTextureSlotNames[] = {
            "Base Color",
            "Diffuse Roughness",
            "Metalness",
            "Occlusion",
            "Emission",
            "Normal"
        };

        for (int i = 0; i < int(ShaderResource::kNumTextures); ++i) {
            auto tex = material->GetTexture(ShaderResource::MaterialTex(i));

            std::string textureName = "None (Texture)";
            if (tex != nullptr) {
                textureName = tex->GetDesc().debugName.empty() ? "Texture" : tex->GetDesc().debugName;
            }

            DrawPropertyRow(kTextureSlotNames[i], [&]() {
                constexpr float previewSize = 96.0f;
                float availWidth = ImGui::GetContentRegionAvail().x;
                if (availWidth > previewSize) {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - previewSize));
                }

                if (tex != nullptr) {
                    auto gpuHandle = tex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
                    ImGui::Image(ImTextureRef{gpuHandle}, ImVec2{previewSize, previewSize});
                }
                else {
                    ImGui::BeginDisabled();
                    ImGui::Button("None", ImVec2{previewSize, previewSize});
                    ImGui::EndDisabled();
                }
            });
        }

        ImGui::PopID();
        ImGui::Unindent();
        ImGui::TreePop();
    }

    inline bool DrawBoolControl(const char* label, bool value, float columnWidth = 120.0f)
    {
        bool edited = value;
        DrawPropertyRow(label, [&]() {
            ImGui::Checkbox("##Value", &edited);
        }, columnWidth);
        return edited;
    }

    inline bool DrawFloatControl(
        const char* label,
        float value,
        float speed = 0.1f,
        float minValue = 0.0f,
        float maxValue = 0.0f,
        const char* format = "%.2f",
        float columnWidth = 120.0f)
    {
        float edited = value;
        DrawPropertyRow(label, [&]() {
            ImGui::DragFloat("##Value", &edited, speed, minValue, maxValue, format);
        }, columnWidth);
        return edited;
    }

    inline bool DrawSliderFloatControl(
        const char* label,
        float value,
        float minValue,
        float maxValue,
        const char* format = "%.2f",
        float columnWidth = 120.0f)
    {
        float edited = value;
        DrawPropertyRow(label, [&]() {
            ImGui::SliderFloat("##Value", &edited, minValue, maxValue, format);
        }, columnWidth);
        return edited;
    }

    struct IComponentDrawer
    {
        virtual ~IComponentDrawer() = default;

        virtual bool CanDraw(const std::shared_ptr<GameObject>& object) { return false; };
        virtual const char* GetName() { return "Component"; };
        virtual bool HasEnableToggle(const std::shared_ptr<GameObject>& object) { return false; };
        virtual bool GetEnabled(const std::shared_ptr<GameObject>& object) { return true; };
        virtual void SetEnabled(const std::shared_ptr<GameObject>& object, bool enabled) {};
        virtual void DrawUI(const std::shared_ptr<GameObject>& object) = 0;
        virtual void AddComponent(const std::shared_ptr<GameObject>& object) = 0;
        virtual void RemoveComponent(const std::shared_ptr<GameObject>& object) = 0;
    };


    struct TransformDrawer : public IComponentDrawer
    {
        bool CanDraw(const std::shared_ptr<GameObject>& object) override
        {
            return object->HasComponent<TransformComponent>();
        }
        const char* GetName() override { return "Transform"; }

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

            float innerAngle = light.GetInnerAngle();
            float editedInnerAngle = DrawSliderFloatControl("Inner Angle", innerAngle, 0.0f, 180.0f);
            if (editedInnerAngle != innerAngle) {
                light.SetInnerAngle(editedInnerAngle);
            }

            float outerAngle = light.GetOuterAngle();
            float editedOuterAngle = DrawSliderFloatControl("Outer Angle", outerAngle, editedInnerAngle, 180.0f);
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

    struct MeshRendererDrawer : public IComponentDrawer
    {
        bool CanDraw(const std::shared_ptr<GameObject>& object) override
        {
            return object->HasComponent<MeshRenderer>();
        }
        
        const char* GetName() override { return "Mesh Renderer"; }

        bool HasEnableToggle(const std::shared_ptr<GameObject>& object) override
        {
            return object != nullptr && object->HasComponent<MeshRenderer>();
        }

        bool GetEnabled(const std::shared_ptr<GameObject>& object) override
        {
            if (object == nullptr) return true;
            auto renderer = object->GetComponent<MeshRenderer>();
            return renderer == nullptr ? true : renderer->IsEnabled();
        }

        void SetEnabled(const std::shared_ptr<GameObject>& object, bool enabled) override
        {
            if (object == nullptr) return;
            if (auto renderer = object->GetComponent<MeshRenderer>(); renderer != nullptr) {
                renderer->SetEnabled(enabled);
            }
        }

        void DrawUI(const std::shared_ptr<GameObject>& object) override
        {
            DSM_CORE_ASSERT(object != nullptr);
            DSM_CORE_ASSERT(object->HasComponent<MeshRenderer>());

            auto& meshRenderer = *object->GetComponent<MeshRenderer>();
            ImGui::PushItemWidth(-FLT_MIN);
            uint32_t renderLayer = meshRenderer.GetRenderLayer();
            DrawPropertyRow("Render Layer", [&]() {
                if (ImGui::InputScalar("##Value", ImGuiDataType_U32, &renderLayer)) {
                    meshRenderer.SetRenderLayer(renderLayer);
                }
            });

            bool castShadow = meshRenderer.CastShadow();
            bool editedCastShadow = DrawBoolControl("Cast Shadow", castShadow);
            if (editedCastShadow != castShadow) {
                meshRenderer.SetCastShadow(editedCastShadow);
            }

            bool receiveShadow = meshRenderer.ReceiveShadow();
            bool editedReceiveShadow = DrawBoolControl("Receive Shadow", receiveShadow);
            if (editedReceiveShadow != receiveShadow) {
                meshRenderer.SetReceiveShadow(editedReceiveShadow);
            }

            auto drawBoundsEditor = [](const char* label, const Math::AxisAlignedBox& box, auto&& setBox) {
                ImGui::PushID(label);
                ImGui::Separator();
                bool opened = ImGui::TreeNodeEx("##BoundsFoldout", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen, "%s", label);

                float minVal[3] = {
                    float(box.GetMin().Get(0)),
                    float(box.GetMin().Get(1)),
                    float(box.GetMin().Get(2))
                };
                float maxVal[3] = {
                    float(box.GetMax().Get(0)),
                    float(box.GetMax().Get(1)),
                    float(box.GetMax().Get(2))
                };

                bool changed = false;

                if (opened) {
                    ImGui::Indent();
                    ImGui::Columns(2, nullptr, false);
                    ImGui::SetColumnWidth(0, 120.0f);
                    ImGui::Text("Min");
                    ImGui::NextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    changed |= ImGui::DragFloat3("##Min", minVal, 0.01f);
                    ImGui::NextColumn();

                    ImGui::Text("Max");
                    ImGui::NextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    changed |= ImGui::DragFloat3("##Max", maxVal, 0.01f);

                    ImGui::Columns(1);
                    ImGui::Unindent();
                    ImGui::TreePop();
                }

                if (changed) {
                    setBox(Math::AxisAlignedBox{
                        Math::Vector3{minVal[0], minVal[1], minVal[2]},
                        Math::Vector3{maxVal[0], maxVal[1], maxVal[2]}
                    });
                }
                ImGui::PopID();
            };

            drawBoundsEditor("Bounds", meshRenderer.GetBounds(), [&meshRenderer](const Math::AxisAlignedBox& bounds) {
                meshRenderer.SetBounds(bounds);
            });
            drawBoundsEditor("Local Bounds", meshRenderer.GetLocalBounds(), [&meshRenderer](const Math::AxisAlignedBox& bounds) {
                meshRenderer.SetLocalBounds(bounds);
            });

            DrawMaterialUI(meshRenderer.GetMaterial());
            ImGui::PopItemWidth();
        }

        void AddComponent(const std::shared_ptr<GameObject>& object) override
        {
            assert(object != nullptr);
            if (!object->HasComponent<MeshRenderer>()) {
                object->AddComponent<MeshRenderer>();
            }
        }
      
        void RemoveComponent(const std::shared_ptr<GameObject>& object)
        {
            assert(object != nullptr);
            if (object->HasComponent<MeshRenderer>()) {
                object->RemoveComponent<MeshRenderer>();
            }
        }
    };


    class ComponentDrawerManager
    {
    public:
        ComponentDrawerManager()
        {
            m_Drawers.push_back(std::make_unique<TransformDrawer>());
            m_Drawers.push_back(std::make_unique<CameraDrawer>());
            m_Drawers.push_back(std::make_unique<MeshRendererDrawer>());
            m_Drawers.push_back(std::make_unique<LightDrawer>());
            m_Drawers.push_back(std::make_unique<NativeScriptDrawer>());
        }

        void DrawComponentsUI(const std::shared_ptr<GameObject>& object)
        {
            ImGui::Separator();

            // Object metadata (Unity-like top area)
            bool objectEnabled = object->IsEnabled();
            if (ImGui::Checkbox("##ObjEnabled", &objectEnabled)) {
                object->SetEnabled(objectEnabled);
            }
            ImGui::SameLine();
            auto& name = object->GetName();
            std::array<char, 256> buffer{};
            const size_t nameCount = (std::min)(name.size(), buffer.size() - 1);
            std::ranges::copy_n(name.begin(), nameCount, buffer.begin());
            const float nameWidth = std::max(140.0f, ImGui::GetContentRegionAvail().x * 0.9f);
            ImGui::SetNextItemWidth(nameWidth);
            if (ImGui::InputText("##Name", buffer.data(), buffer.size())) {
                name = std::string{buffer.data()};
            }

            auto& tag = object->GetTag();
            std::array<char, 256> tagBuffer{};
            const size_t tagCount = (std::min)(tag.size(), tagBuffer.size() - 1);
            std::ranges::copy_n(tag.begin(), tagCount, tagBuffer.begin());

            ImGui::TextUnformatted("Tag");
            ImGui::SameLine();
            const float tagWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x * 0.45f);
            ImGui::SetNextItemWidth(tagWidth);
            if (ImGui::InputText("##Tag", tagBuffer.data(), tagBuffer.size())) {
                tag = std::string{tagBuffer.data()};
            }

            ImGui::Separator();

            // 绘制各个组件的 UI
            for(auto& drawer : m_Drawers){
                if(!drawer->CanDraw(object))
                    continue;

                const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
		
                ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4,4});
                float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
                ImGui::Separator();
                auto name = drawer->GetName();
                ImGui::PushID(name);

                if (drawer->HasEnableToggle(object)) {
                    bool enabled = drawer->GetEnabled(object);
                    if (ImGui::Checkbox("##ComponentEnabled", &enabled)) {
                        drawer->SetEnabled(object, enabled);
                    }
                    ImGui::SameLine();
                }

                bool opened = ImGui::TreeNodeEx("##ComponentHeader", treeNodeFlags, "%s", name);
                ImGui::PopStyleVar();
                ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);

                if(ImGui::Button("...", ImVec2{lineHeight, lineHeight})){
                    ImGui::OpenPopup("ComponentSettings");
                }

                bool removeComponent = false;
                if(ImGui::BeginPopup("ComponentSettings")){
                    if(ImGui::MenuItem("Remove Component")){
                        removeComponent = true;
                    }
                    ImGui::EndPopup();
                }
                
                if(opened){
                    drawer->DrawUI(object);
                    ImGui::TreePop();
                }

                if(removeComponent){
                    drawer->RemoveComponent(object);
                }

                ImGui::PopID();

                ImGui::Spacing();
            }

            ImGui::Separator();
            const float addBtnWidth = 180.0f;
            const float availWidth = ImGui::GetContentRegionAvail().x;
            const float cursorX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(cursorX + std::max(0.0f, (availWidth - addBtnWidth) * 0.5f));

            if(ImGui::Button("Add Component", ImVec2{addBtnWidth, 0.0f})){
                ImGui::OpenPopup("AddComponent");
            }

            if(ImGui::BeginPopup("AddComponent")){
                for(auto& drawer : m_Drawers){
                    auto name = drawer->GetName();
                    if (ImGui::MenuItem(name)) {
                        drawer->AddComponent(object);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }
        }

    private:
        std::vector<std::unique_ptr<IComponentDrawer>> m_Drawers;
    };
}

#endif // __COMPONENT_DRAWER_H__
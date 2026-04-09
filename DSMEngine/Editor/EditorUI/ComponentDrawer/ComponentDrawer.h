#pragma once
#ifndef __COMPONENT_DRAWER_H__
#define __COMPONENT_DRAWER_H__

#include <imgui_internal.h>

#include "Runtime/Math/MathCommon.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Render/Material.h"
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


    class ComponentDrawerManager
    {
    public:
        ComponentDrawerManager();
        void DrawComponentsUI(const std::shared_ptr<GameObject>& object);

    private:
        std::vector<std::unique_ptr<IComponentDrawer>> m_Drawers;
    };
}

#endif // __COMPONENT_DRAWER_H__

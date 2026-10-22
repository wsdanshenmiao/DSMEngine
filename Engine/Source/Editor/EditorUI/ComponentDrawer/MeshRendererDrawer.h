#pragma once
#ifndef __MESH_RENDERER_DRAWER_H__
#define __MESH_RENDERER_DRAWER_H__

#include "ComponentDrawer.h"
#include "Runtime/Framework/Component/MeshRenderer.h"

namespace DSM {
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

            const auto worldBounds = meshRenderer.GetBounds();
            ImGui::Separator();
            ImGui::TextUnformatted("World Bounds (read only)");
            if (worldBounds.IsValid()) {
                const auto& min = worldBounds.GetMin();
                const auto& max = worldBounds.GetMax();
                ImGui::Text("Min: %.3f, %.3f, %.3f", float(min.Get(0)), float(min.Get(1)), float(min.Get(2)));
                ImGui::Text("Max: %.3f, %.3f, %.3f", float(max.Get(0)), float(max.Get(1)), float(max.Get(2)));
            }
            else ImGui::TextUnformatted("Invalid bounds");
            const auto editableLocalBounds = meshRenderer.GetLocalBounds().IsValid()
                ? meshRenderer.GetLocalBounds()
                : Math::AxisAlignedBox{Math::Vector3{0, 0, 0}, Math::Vector3{1, 1, 1}};
            drawBoundsEditor("Local Bounds", editableLocalBounds, [&meshRenderer, &object](const Math::AxisAlignedBox& bounds) {
                meshRenderer.SetLocalBounds(bounds);
                object->MarkSceneDirty();
            });

            if (auto material = meshRenderer.GetMaterial(0)) DrawMaterialUI(material);
            else ImGui::TextUnformatted("Material: None");
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
}

#endif // __MESH_RENDERER_DRAWER_H__
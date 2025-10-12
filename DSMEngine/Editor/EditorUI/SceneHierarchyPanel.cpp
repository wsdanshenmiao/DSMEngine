#include "SceneHierarchyPanel.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Component/Component.h"
#include <imgui_internal.h>
#include <numbers>

namespace DSM{

    void SceneHierarchyPanel::SetScene(std::shared_ptr<Scene> scene)
    {
        m_Scene = scene;
        m_SelectedObject.reset();
    }
    
    void SceneHierarchyPanel::OnGUI()
    {
        // 场景中的所有物体
        ImGui::Begin("Scene Hierarchy");
        if(m_Scene != nullptr){
            m_Scene->TraverseAllEntity([this](entt::entity entity) {
                std::shared_ptr<GameObject> object = m_Scene->GetObjectByID(entity).lock();
                if (object != nullptr) {
                    DrawEntityNode(object);
                }
            });
        }
        ImGui::End();

        // 属性面板
        ImGui::Begin("Properties");
        if (auto selectedObject = GetSelectedObject().lock(); selectedObject != nullptr) {
            // 显示选中物体的属性
            DrawComponents(selectedObject);
        }
        ImGui::End();
    }

    // 绘制场景中实体的 UI 节点
    void SceneHierarchyPanel::DrawEntityNode(std::shared_ptr<GameObject> object)
    {
        const auto& name = object->GetComponent<TagComponent>()->tag;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if(m_SelectedObject.lock() == object){
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool opened = ImGui::TreeNodeEx(object.get(), flags, name.c_str());
        // 当前节点被选中 
        if(ImGui::IsItemClicked()){
            m_SelectedObject = object;
        }
        
        if(opened){
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::DrawComponents(std::shared_ptr<GameObject> object)
    {
        // Draw the components of the GameObject
        // 实体的 Tag 组件
        if (auto tagComponent = object->GetComponent<TagComponent>(); tagComponent != nullptr) {
            std::array<char, 256> buffer{};
            if(tagComponent->tag.size() > buffer.size()){
                tagComponent->tag.resize(buffer.size());
            }
            std::ranges::copy(tagComponent->tag, buffer.begin());
            if (ImGui::InputText("##Tag", buffer.data(), buffer.size())) {
                tagComponent->tag = std::string{buffer.data()};
            }
        }

        ImGui::SameLine();

        DrawComponent<Math::Transform>("Transform", object, [](Math::Transform& component) {
            Math::Vector3 pos = component.GetPosition();
            Math::Vector3 rot = component.GetRotation().ToEulerAngles();
            Math::Vector3 scale = component.GetScale();
            float factor = 180.0 / std::numbers::pi;
            rot *= factor;
            DrawVec3Control("Position", pos);
            DrawVec3Control("Rotation", rot);
            DrawVec3Control("Scale", scale, 1);
            rot /= factor;
            component.SetPosition(pos);
            component.SetRotation(Math::Quaternion{rot});
            component.SetScale(scale);
        });
    }
    
    void SceneHierarchyPanel::DrawVec3Control(const std::string &label, Math::Vector3 &values, float resetValue, float columnWidth)
    {
		ImGuiIO& io = ImGui::GetIO();
		const auto& boldFont = io.Fonts->Fonts[0];

        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

        auto showVal = [&boldFont, buttonSize, resetValue](
                const char* name, float& val,
                const ImVec4& col0, const ImVec4& col1, const ImVec4& col2) {
        	ImGui::PushStyleColor(ImGuiCol_Button, col0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col1);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, col2);
            ImGui::PushFont(boldFont);
            if (ImGui::Button(name, buttonSize))
                val = resetValue;
            ImGui::PopFont();
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            std::string valLabel = "##" + std::string(name);
            ImGui::DragFloat(valLabel.c_str(), &val, 0.1f, 0.0f, 0.0f, "%.2f");
            ImGui::PopItemWidth();
        };

        float val = values.Get(0);
        showVal("X", val, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f }, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f }, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		values.Set(0, val);
        ImGui::SameLine();

        val = values.Get(1);
		showVal("Y", val, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f }, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f }, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		values.Set(1, val);
		ImGui::SameLine();

        val = values.Get(2);
        showVal("Z", val, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f }, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f }, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		values.Set(2, val);

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
    }
}

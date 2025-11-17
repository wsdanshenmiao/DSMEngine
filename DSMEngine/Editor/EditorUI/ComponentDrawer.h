#pragma once
#ifndef __COMPONENT_DRAWER_H__
#define __COMPONENT_DRAWER_H__

#include <numbers>
#include <imgui.h>
#include <imgui_internal.h>
#include "Runtime/Math/MathCommon.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Render/Camera/Camera.h"
#include "Runtime/Render/ModelLoader.h"


namespace DSM{

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

    inline void DrawMaterial(Material& mat)
    {
        std::array<float, 4> vec4{};

        ImGui::PushID(&mat);

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, 125.0f);

        ImGui::Text("Base Color");
        ImGui::NextColumn();
        memcpy(vec4.data(), &mat.baseColor, sizeof(mat.baseColor));
        if(ImGui::ColorEdit4("##Base Color", vec4.data())){
            memcpy(&mat.baseColor, vec4.data(), sizeof(mat.baseColor));
        }
        ImGui::NextColumn();


        ImGui::Text("Emissive Color");
        ImGui::NextColumn();
        memcpy(vec4.data(), &mat.emissiveColor, sizeof(mat.emissiveColor));
        if(ImGui::ColorEdit3("##Emissive Color", vec4.data())){
            memcpy(&mat.emissiveColor, vec4.data(), sizeof(mat.emissiveColor));
        }
        ImGui::NextColumn();

        ImGui::Text("Normal Texture Scale");
        ImGui::NextColumn();
        ImGui::SliderFloat("##Normal Texture Scale", &mat.normalTexScale, 0.0f, 1.0f);
        ImGui::NextColumn();

        ImGui::Text("Metallic Factor");
        ImGui::NextColumn();
        ImGui::SliderFloat("##Metallic Factor", &mat.metallicFactor, 0.0f, 1.0f);
        ImGui::NextColumn();

        ImGui::Text("Roughness Factor");
        ImGui::NextColumn();
        ImGui::SliderFloat("##Roughness Factor", &mat.roughnessFactor, 0.0f, 1.0f);
        ImGui::NextColumn();

        ImGui::Columns(1);
        ImGui::PopID();
    }

    struct IComponentDrawer
    {
        virtual ~IComponentDrawer() = default;

        virtual bool CanDraw(std::shared_ptr<GameObject> object) { return false; };
        virtual std::string GetName() { return "Component"; };
        virtual void DrawUI(std::shared_ptr<GameObject> object) = 0;
        virtual void AddComponent(std::shared_ptr<GameObject> object) = 0;
        virtual void RemoveComponent(std::shared_ptr<GameObject> object) = 0;
    };


    struct TransformComponentDrawer : public IComponentDrawer
    {
        bool CanDraw(std::shared_ptr<GameObject> object) override
        {
            return object->HasComponent<Math::Transform>();
        }
        std::string GetName() override { return "Transform"; }

        void DrawUI(const std::shared_ptr<GameObject> object) override
        {
            assert(object != nullptr);

            const auto& component = object->GetComponent<Math::Transform>();
            Math::Vector3 pos = component->GetPosition();
            Math::Vector3 scale = component->GetScale();
            Math::Vector3 degree = Math::RadiansToDegree(component->GetRotation().ToEulerAngles());
            DrawVec3Control("Position", pos);
            DrawVec3Control("Rotation", degree);
            DrawVec3Control("Scale", scale, 1);
            component->SetPosition(pos);
            component->SetRotation(Math::Quaternion{Math::DegreeToRadians(degree)});
            component->SetScale(scale);
        }
        void AddComponent(std::shared_ptr<GameObject> object) override
        {
            assert(object != nullptr);

            if (!object->HasComponent<Math::Transform>()) {
                object->AddComponent<Math::Transform>();
            }
        }
        void RemoveComponent(std::shared_ptr<GameObject> object)
        {
            assert(object != nullptr);

            if (object->HasComponent<Math::Transform>()) {
                object->RemoveComponent<Math::Transform>();
            }
        }
    };

    struct CameraComponentDrawer : public IComponentDrawer
    {
        bool CanDraw(std::shared_ptr<GameObject> object) override
        {
            return object->HasComponent<Camera>();
        }
        std::string GetName() override { return "Camera"; }
        void DrawUI(const std::shared_ptr<GameObject> object) override
        {
            assert(object != nullptr);

            const auto& camera = object->GetComponent<Camera>();

            if(float cameraFov = Math::RadiansToDegree(camera->GetFovY());
                ImGui::DragFloat("Vertical Fov", &cameraFov, 1, 0, 360)){
                camera->SetFovY(Math::DegreeToRadians(cameraFov));
            }
            float nearZ = camera->GetNearZ();
            if(ImGui::DragFloat("Near Plane", &nearZ, 1, 0.001, std::numeric_limits<float>::max())){
                camera->SetNearZ(nearZ);
            }
            if(float farZ = camera->GetFarZ();
                ImGui::DragFloat("Far Plane", &farZ, 1, nearZ, std::numeric_limits<float>::max())){
                camera->SetFarZ(std::max(nearZ, farZ));
            }
            if(bool reverse = camera->IsReversedZ();
                ImGui::Checkbox("Reverse Z", &reverse)){
                camera->ReverseZ(reverse);
            }
        }
        void AddComponent(std::shared_ptr<GameObject> object) override
        {
            assert(object != nullptr);
            if (!object->HasComponent<Camera>()) {
                object->AddComponent<Camera>();
            }
        }
        void RemoveComponent(std::shared_ptr<GameObject> object)
        {
            assert(object != nullptr);
            if (object->HasComponent<Camera>()) {
                object->RemoveComponent<Camera>();
            }
        }
    };

    struct ModelDrawer : public IComponentDrawer
    {
        bool CanDraw(std::shared_ptr<GameObject> object) override
        {
            return object->HasComponent<Model>();
        }
        std::string GetName() override { return "Model"; }
        void DrawUI(const std::shared_ptr<GameObject> object) override
        {
            assert(object != nullptr);

            const auto& model = object->GetComponent<Model>();
            std::array<char, 256> buffer{};
            if(model->name.size() > buffer.size()){
                model->name.resize(buffer.size());
            }
            std::ranges::copy(model->name, buffer.begin());
            if (ImGui::InputText("Model Name", buffer.data(), buffer.size())) {
                model->name = std::string{buffer.data()};
            }

            std::string filePath = model->filePath;
            if(model->filePath.size() > buffer.size()){
                model->filePath.resize(buffer.size());
            }
            std::ranges::copy(model->filePath, buffer.begin());
            if (ImGui::InputText("Model File Path", buffer.data(), buffer.size())) {
                model->filePath = std::string(buffer.data());
                auto newModel = ModelLoader::LoadModel(model->filePath);
                if(newModel != nullptr){
                    *model = std::move(*newModel);
                }
            }

            for(const auto& material : model->materials){
                DrawMaterial(*material);
            }
        }
        void AddComponent(std::shared_ptr<GameObject> object) override
        {
            assert(object != nullptr);
            if (!object->HasComponent<Model>()) {
                object->AddComponent<Model>();
            }
        }
        void RemoveComponent(std::shared_ptr<GameObject> object)
        {
            assert(object != nullptr);
            if (object->HasComponent<Model>()) {
                object->RemoveComponent<Model>();
            }
        }
    };

    class ComponentDrawerManager
    {
    public:
        ComponentDrawerManager()
        {
            m_Drawers.push_back(std::make_unique<TransformComponentDrawer>());
            m_Drawers.push_back(std::make_unique<CameraComponentDrawer>());
            m_Drawers.push_back(std::make_unique<ModelDrawer>());
        }

        void DrawComponentsUI(std::shared_ptr<GameObject> object)
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
            // 组件的添加
            {
                ImGui::PushItemWidth(-1);

                if(ImGui::Button("Add Component")){
                    ImGui::OpenPopup("AddComponent");
                }

                if(ImGui::BeginPopup("AddComponent")){
                    for(auto& drawer : m_Drawers){
                        auto name = drawer->GetName();
                        if (ImGui::MenuItem(name.c_str())) {
                            drawer->AddComponent(object);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopItemWidth();
            }

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
                bool opened = ImGui::TreeNodeEx(name.c_str(), treeNodeFlags, name.c_str());
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
            }
        }

    private:
        std::vector<std::unique_ptr<IComponentDrawer>> m_Drawers;
    };
}

#endif // __COMPONENT_DRAWER_H__
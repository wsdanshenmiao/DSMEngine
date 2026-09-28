#include "ComponentDrawer.h"
#include "LightDrawer.h"
#include "NativeScriptDrawer.h"
#include "MeshRendererDrawer.h"
#include "CameraDrawer.h"
#include "TransformDrawer.h"

namespace DSM {
    ComponentDrawerManager::ComponentDrawerManager()
    {
        m_Drawers.emplace_back(std::make_unique<TransformDrawer>());
        m_Drawers.emplace_back(std::make_unique<CameraDrawer>());
        m_Drawers.emplace_back(std::make_unique<LightDrawer>());
        m_Drawers.emplace_back(std::make_unique<MeshRendererDrawer>());
        m_Drawers.emplace_back(std::make_unique<NativeScriptDrawer>());
    }
    
    void ComponentDrawerManager::DrawComponentsUI(const std::shared_ptr<GameObject> &object)
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
}
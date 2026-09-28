#pragma once
#ifndef __EDITORUI_H__
#define __EDITORUI_H__

#include <memory>

#include <imgui.h>

#include "Runtime/Render/WindowUI.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Render/Mesh.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Editor/EditorUI/Widget.h"
#include "Editor/EditorUI/EditorMenuBar.h"

namespace DSM {
    class Window;
    class GraphicsRenderer;

    struct EditorUIDesc
    {
        std::shared_ptr<GraphicsRenderer> renderer;
        std::shared_ptr<Window> window;
    };

    class EditorUI : public WindowUI
    {
        friend class DSMEditor;
    public:
        EditorUI(DSMEditor* editor);
        ~EditorUI() override;

        void OnGUI() override;
        void OnEvent(Event& event) override;

        const EditorMenuBar& GetMenuBar() const { return *m_MenuBar; }

        DSMEditor* GetEditor() const { return m_Editor; }

        template <typename T>
        T* GetWidget()
        {
            for (const auto& widget : m_Widgets) {
                if (T* castedWidget = dynamic_cast<T*>(widget.get())) {
                    return castedWidget;
                }
            }
            return nullptr;
        }

    private:
        bool DrawProjectGateModal();
        void OnScenePlay();
        void OnSceneStop();

    private:
        DSMEditor* m_Editor;

        std::vector<std::unique_ptr<Widget>> m_Widgets;
        std::unique_ptr<EditorMenuBar> m_MenuBar;

        std::shared_ptr<Scene> m_InactiveScene;

        ImFont* m_Font;
    };


    inline void ConvertModelToGameObject(std::shared_ptr<Model> model, Scene& scene)
    {
        if(model == nullptr)
            return;

        auto rootObject = scene.GetObjectByID(scene.CreateObject(model->name)).lock();
        for(size_t i = 0; i < model->meshes.size(); ++i){
            auto& mesh = model->meshes[i];
            auto& materialIndices = model->meshMaterialIndices[i];
            auto child = scene.GetObjectByID(scene.CreateObject(mesh->GetName())).lock();
            auto meshRenderer = child->AddComponent<MeshRenderer>();
            meshRenderer->SetMesh(mesh);
            meshRenderer->SetModel(model);
            std::map<std::shared_ptr<Material>, size_t> materials{};
            std::vector<std::shared_ptr<Material>> meshMaterials{};
            for(const auto& [i, matIndex] : materialIndices | std::views::enumerate) {
                if(auto modelMats = model->materials; matIndex < modelMats.size()){
                    size_t index = 0;
                    if(materials.contains(modelMats[matIndex])){
                        index = materials[modelMats[matIndex]];
                    }
                    else{
                        index = materials.size();
                        materials[modelMats[matIndex]] = index;
                        meshMaterials.push_back(modelMats[matIndex]);
                    }
                    meshRenderer->SetMaterialIndex(i, index);
                }
            }
			meshRenderer->SetMaterials(std::move(meshMaterials));

            rootObject->AddChild(child);
        }
    }
    
} // namespace DSM 

#endif
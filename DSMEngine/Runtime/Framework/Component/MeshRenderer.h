#pragma once
#ifndef __MESHRENDERER_H__
#define __MESHRENDERER_H__

#include "Runtime/Render/Mesh.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Framework/Component/Renderer.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/TransformComponent.h"

namespace DSM {
    class MeshRenderer : public Renderer
    {
    public:
        MeshRenderer(std::shared_ptr<GameObject> gameObject)
            : Renderer(gameObject) {}
        virtual ~MeshRenderer() = default;

        const std::shared_ptr<Mesh>& GetMesh() const noexcept { return m_Mesh; }
        void SetMesh(const std::shared_ptr<Mesh>& mesh) noexcept
        { 
            m_Mesh = mesh;
            SetLocalBounds(mesh->bounds);
            if(auto obj = m_GameObject.lock(); obj != nullptr){
                auto transform = obj->GetComponent<TransformComponent>();
                if(transform != nullptr){
                    auto worldBounds = mesh->bounds * *transform;
                    SetBounds(worldBounds);
                }
			}
            m_IsDirty = true;
        }

        std::shared_ptr<Model> GetModel() const noexcept { return m_Model; }
        void SetModel(const std::shared_ptr<Model>& model) noexcept{ m_Model = model; m_IsDirty = true; }
        
        size_t GetMaterialIndex(size_t subMeshIndex) const noexcept
        {
            if(subMeshIndex >= m_SubMeshMaterialIndices.size()) {
                DSM_CORE_ASSERT(subMeshIndex < m_SubMeshMaterialIndices.size(), "subMeshIndex out of range");
                return 0;
            }
            return m_SubMeshMaterialIndices[subMeshIndex];
        }
        void SetMaterialIndex(size_t subMeshIndex, size_t materialIndex) noexcept
        {
            if(subMeshIndex >= m_SubMeshMaterialIndices.size()) {
                m_SubMeshMaterialIndices.resize(subMeshIndex + 1, 0);
            }
            m_SubMeshMaterialIndices[subMeshIndex] = materialIndex;
            m_IsDirty = true;
        }

    private:
        std::shared_ptr<Mesh> m_Mesh{};
        std::shared_ptr<Model> m_Model{};
        std::vector<size_t> m_SubMeshMaterialIndices{};
    };
} // namespace DSM



#endif // !__MESHRENDERER_H__
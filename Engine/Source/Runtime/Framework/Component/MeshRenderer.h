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
        Math::AxisAlignedBox GetBounds() const noexcept
        {
            if (!m_LocalBounds.IsValid()) return {};
            auto obj = m_GameObject.lock();
            auto transform = obj != nullptr ? obj->GetComponent<TransformComponent>() : nullptr;
            return transform != nullptr ? m_LocalBounds * transform->GetLocalToWorld() : Math::AxisAlignedBox{};
        }
        void SetLocalBounds(const Math::AxisAlignedBox& bounds) noexcept
        {
            Renderer::SetLocalBounds(bounds);
        }
        void SetMesh(const std::shared_ptr<Mesh>& mesh) noexcept
        {
            m_Mesh = mesh;
            SetLocalBounds(mesh != nullptr ? mesh->bounds : Math::AxisAlignedBox{});
            m_IsDirty = true;
        }

        std::shared_ptr<Model> GetModel() const noexcept { return m_Model; }
        void SetModel(const std::shared_ptr<Model>& model) noexcept{ m_Model = model; m_IsDirty = true; }
        
        size_t GetMaterialIndex(size_t subMeshIndex) const noexcept
        {
            return GetMaterialIndexOrDefault(subMeshIndex);
        }
        size_t GetMaterialIndexOrDefault(size_t subMeshIndex) const noexcept
        {
            return subMeshIndex < m_SubMeshMaterialIndices.size()
                ? m_SubMeshMaterialIndices[subMeshIndex]
                : 0;
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

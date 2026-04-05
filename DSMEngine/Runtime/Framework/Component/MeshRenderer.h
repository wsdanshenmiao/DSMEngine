#pragma once
#ifndef __MESHRENDERER_H__
#define __MESHRENDERER_H__

#include "Runtime/Framework/Component/Renderer.h"
#include "Runtime/Render/Mesh.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/TransformComponent.h"

namespace DSM {
    class MeshRenderer : public Renderer
    {
    public:
        MeshRenderer(std::shared_ptr<GameObject> gameObject)
            : Renderer(gameObject) {}
        virtual ~MeshRenderer() = default;
    
        void SetMesh(const std::shared_ptr<Mesh>& mesh) noexcept
        { 
            m_Mesh = mesh;
            if (m_Material != nullptr) {
                for(size_t i = 0; i < mesh->textures.size(); i++){
                    m_Material->SetTexture(ShaderResource::MaterialTex(i), mesh->textures[i]);
				}
            }
            SetLocalBounds(mesh->boundingBox);
            if(auto obj = m_GameObject.lock(); obj != nullptr){
                auto transform = obj->GetComponent<TransformComponent>();
                if(transform != nullptr){
                    auto worldBounds = mesh->boundingBox * *transform;
                    SetBounds(worldBounds);
                }
			}
        }
        const std::shared_ptr<Mesh>& GetMesh() const noexcept { return m_Mesh; }

    private:
        std::shared_ptr<Mesh> m_Mesh{};
    };
} // namespace DSM



#endif // !__MESHRENDERER_H__
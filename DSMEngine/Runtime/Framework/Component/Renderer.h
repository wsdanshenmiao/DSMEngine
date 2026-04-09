#pragma once
#ifndef __RENDERER_H__
#define __RENDERER_H__

#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Math/Collision/BoundingBox.h"
#include "Runtime/Render/Material.h"

namespace DSM {
    class Renderer : public IComponent
    {
    public:
        Renderer(std::shared_ptr<GameObject> gameObject)
            : IComponent(gameObject) {}
        virtual ~Renderer() = default;

        const Math::AxisAlignedBox& GetBounds() const noexcept { return m_Bouns; }
        const Math::AxisAlignedBox& GetLocalBounds() const noexcept { return m_LocalBounds; }
		void SetBounds(const Math::AxisAlignedBox& bounds) noexcept { m_Bouns = bounds; }
		void SetLocalBounds(const Math::AxisAlignedBox& localBounds) noexcept { m_LocalBounds = localBounds; }

        std::shared_ptr<Material> GetMaterial(size_t index) const noexcept { return m_Materials[index]; }
        const std::vector<std::shared_ptr<Material>>& GetMaterials() const noexcept { return m_Materials; }
        void SetMaterial(size_t index, const std::shared_ptr<Material>& material) noexcept
        { 
            if(index >= m_Materials.size()) {
                m_Materials.resize(index + 1);
            }
            m_Materials[index] = material;
        }
        void SetMaterials(std::vector<std::shared_ptr<Material>> materials) noexcept { m_Materials = std::move(materials); }

        uint32_t GetRenderLayer() const noexcept { return m_RenderLayer; }
        void SetRenderLayer(uint32_t layer) noexcept { m_RenderLayer = layer; }
        
        bool CastShadow() const noexcept { return m_CastShadow; }
        void SetCastShadow(bool castShadow) noexcept { m_CastShadow = castShadow; }
        bool ReceiveShadow() const noexcept { return m_ReceiveShadow; }
        void SetReceiveShadow(bool receiveShadow) noexcept { m_ReceiveShadow = receiveShadow; }
        
        bool IsEnabled() const noexcept { return m_Enabled; }
        void SetEnabled(bool enabled) noexcept { m_Enabled = enabled; }

    protected:
        // 世界空间的包围盒
        Math::AxisAlignedBox m_Bouns{};
        // 局部空间的包围盒
        Math::AxisAlignedBox m_LocalBounds{};
        std::vector<std::shared_ptr<Material>> m_Materials{};
        uint32_t m_RenderLayer = 0;
        bool m_CastShadow = true;
        bool m_ReceiveShadow = true;
        bool m_Enabled = true;
    };
}
#endif // !__RENDERER_H__
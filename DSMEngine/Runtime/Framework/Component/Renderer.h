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

        std::shared_ptr<Material> GetMaterial() const noexcept { return m_Material; }
        void SetMaterial(const std::shared_ptr<Material>& material) noexcept { m_Material = material; }

        uint32_t GetRenderLayer() const noexcept { return m_RenderLayer; }
        
        bool CastShadow() const noexcept { return m_CastShadow; }
        bool ReceiveShadow() const noexcept { return m_ReceiveShadow; }
        
        bool IsEnabled() const noexcept { return m_Enabled; }

    protected:
        // 世界空间的包围盒
        Math::AxisAlignedBox m_Bouns{};
        // 局部空间的包围盒
        Math::AxisAlignedBox m_LocalBounds{};
        std::shared_ptr<Material> m_Material{};
        uint32_t m_RenderLayer = 0;
        bool m_CastShadow = true;
        bool m_ReceiveShadow = true;
        bool m_Enabled = true;
    };
}
#endif // !__RENDERER_H__
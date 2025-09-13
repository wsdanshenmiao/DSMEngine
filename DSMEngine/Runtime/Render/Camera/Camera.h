#pragma once
#ifndef __CAMERA__H__
#define __CAMERA__H__

#include <numbers>
#include "Runtime/Graphics/GraphicsCommon.h"
#include "Runtime/Math/Transform.h"

namespace DSM {
    class Camera
    {
    public:
		const Math::Transform& GetTransform() const noexcept { return m_Transform; }

        Math::Vector3 GetPosition() const noexcept { return m_Transform.GetPosition(); }
        
        Math::Matrix4 GetViewMatrix() const noexcept { return m_Transform.GetWorldToLocal(); }
        Math::Matrix4 GetProjMatrix() const noexcept
        {
            return Math::GetProjMatrix(m_FovY, m_Aspect, m_ReversedZ ? m_FarZ : m_NearZ, m_ReversedZ ? m_NearZ : m_FarZ);
        }
        Math::Matrix4 GetViewProjMatrix() const noexcept { return GetViewMatrix() * GetProjMatrix(); }

        Math::Vector3 GetRightAxis() const noexcept { return m_Transform.GetRightAxis(); }
        Math::Vector3 GetUpAxis() const noexcept { return m_Transform.GetUpAxis(); }
        Math::Vector3 GetLookAxis() const noexcept { return m_Transform.GetForwardAxis(); }

        const Viewport& GetViewPort() const noexcept { return m_Viewport; }
        float GetNearZ() const noexcept { return m_NearZ; }
        float GetFarZ() const noexcept { return m_FarZ; }
        float GetFovY() const noexcept { return m_FovY; }
        float GetAspectRatio() const noexcept { return m_Aspect; }

        void SetPosition(float x, float y, float z) noexcept { m_Transform.SetPosition(x, y, z); }
        void SetPosition(Math::Vector3 position) noexcept { m_Transform.SetPosition(position); }
        void LookAt(Math::Vector3 target,Math::Vector3 up) noexcept { m_Transform.LookAt(target, up); }
        void LookTo(Math::Vector3 to, Math::Vector3 up) noexcept { m_Transform.LookTo(to, up); }
        void RotateX(float angle) noexcept { m_Transform.Rotate(angle, 0, 0); }
        void RotateY(float angle) noexcept { m_Transform.Rotate(0, angle, 0); }

        void Translate(Math::Vector3 translation) noexcept { m_Transform.Translate(translation); }

        // 设置视口
        void SetViewPort(const Viewport& viewport) noexcept { m_Viewport = viewport; }
        void SetViewPort(
            float topLeftX, float topLeftY,
            float width, float height,
            float minDepth = 0.0f, float maxDepth = 1.0f) noexcept
        {
            m_Viewport = {topLeftX, topLeftY, width, height, minDepth, maxDepth};
        }

        void SetFrustum(float fovY, float aspect, float nearZ, float farZ) 
        {
			m_FovY = fovY;
			m_Aspect = aspect;
			m_NearZ = nearZ;
			m_FarZ = farZ;
        }

        void ReverseZ(bool enable) { m_ReversedZ = enable; }
    
    protected:
        Math::Transform m_Transform{};
        Viewport m_Viewport{};
        float m_NearZ = 0.1f;
        float m_FarZ = 1000.0f;
        float m_Aspect = 1.0f;
        float m_FovY = std::numbers::pi * 0.5f;

        bool m_ReversedZ = false;
    };

    
}

#endif
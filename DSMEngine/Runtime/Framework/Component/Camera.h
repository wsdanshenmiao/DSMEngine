#pragma once
#ifndef __CAMERA__H__
#define __CAMERA__H__

#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Math/Collision/Frustum.h"
#include "Runtime/Graphics/GraphicsCommon.h"
#include "Runtime/Framework/Component/Transform.h"
#include "Runtime/Framework/Object/GameObject.h"

namespace DSM {
    class Camera : public IComponent
    {
    public:
        Camera() : m_Transform(new Transform()) {}
        Camera(std::shared_ptr<GameObject> gameObject)
            : IComponent(gameObject), m_Transform(gameObject->GetComponent<Transform>()) {}
        virtual ~Camera()
        {
            if(m_GameObject.expired() && m_Transform != nullptr)
                delete m_Transform;
        }

        Math::Vector3 GetPosition() const noexcept { return m_Transform->GetPosition(); }
        Math::Quaternion GetRotation() const noexcept { return m_Transform->GetRotation(); }
        
        Math::Matrix4 GetViewMatrix() const noexcept { return m_Transform->GetWorldToLocal(); }
        Math::Matrix4 GetProjMatrix() const noexcept
        {
            return Math::GetProjMatrix(m_FovY, m_Aspect, m_ReversedZ ? m_FarZ : m_NearZ, m_ReversedZ ? m_NearZ : m_FarZ);
        }
        Math::Matrix4 GetViewProjMatrix() const noexcept { return GetViewMatrix() * GetProjMatrix(); }

        Math::Vector3 GetRightAxis() const noexcept { return m_Transform->GetRightAxis(); }
        Math::Vector3 GetUpAxis() const noexcept { return m_Transform->GetUpAxis(); }
        Math::Vector3 GetLookAxis() const noexcept { return m_Transform->GetForwardAxis(); }

        const Viewport& GetViewPort() const noexcept { return m_Viewport; }
        float GetNearZ() const noexcept { return m_NearZ; }
        float GetFarZ() const noexcept { return m_FarZ; }
        float GetFovY() const noexcept { return m_FovY; }
        float GetAspectRatio() const noexcept { return m_Aspect; }
        Math::Frustum GetFrustum() const noexcept { return Math::Frustum{GetProjMatrix()}; }

        void SetPosition(float x, float y, float z) noexcept { m_Transform->SetPosition(x, y, z); }
        void SetPosition(Math::Vector3 position) noexcept { m_Transform->SetPosition(position); }
        void SetRotation(float pitch, float yaw, float roll) noexcept { m_Transform->SetRotation(pitch, yaw, roll); }
        void SetRotation(Math::Quaternion rotation) noexcept { m_Transform->SetRotation(rotation); }

        void LookAt(Math::Vector3 target,Math::Vector3 up) noexcept { m_Transform->LookAt(target, up); }
        void LookTo(Math::Vector3 to, Math::Vector3 up) noexcept { m_Transform->LookTo(to, up); }
        void RotateX(float angle) noexcept { m_Transform->Rotate(angle, 0, 0); }
        void RotateY(float angle) noexcept { m_Transform->Rotate(0, angle, 0); }

        void Translate(Math::Vector3 translation) noexcept { m_Transform->Translate(translation); }

        // 设置视口
        void SetViewPort(const Viewport& viewport) noexcept { m_Viewport = viewport; }
        void SetViewPort(
            float topLeftX, float topLeftY,
            float width, float height,
            float minDepth = 0.0f, float maxDepth = 1.0f) noexcept
        {
            m_Viewport = {topLeftX, topLeftY, width, height, minDepth, maxDepth};
        }

        void SetFovY(float fovY) noexcept { m_FovY = fovY; }
        void SetNearZ(float nearZ) noexcept { m_NearZ = nearZ; }
        void SetFarZ(float farZ) noexcept { m_FarZ = farZ; }

        void SetFrustum(float fovY, float aspect, float nearZ, float farZ) 
        {
			m_FovY = fovY;
			m_Aspect = aspect;
			m_NearZ = nearZ;
			m_FarZ = farZ;
        }

        void ReverseZ(bool enable) { m_ReversedZ = enable; }
        bool IsReversedZ() const { return m_ReversedZ; }
    
    protected:
        Transform* m_Transform{};
        Viewport m_Viewport{};
        float m_NearZ = 0.1f;
        float m_FarZ = 1000.0f;
        float m_Aspect = 1.0f;
        float m_FovY = std::numbers::pi * 0.5f;

        bool m_ReversedZ = false;
    };
} // namespace DSM



#endif // __CAMERA__H__
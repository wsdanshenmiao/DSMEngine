#pragma once
#ifndef __TRANSFORMCOMPONENT_H__
#define __TRANSFORMCOMPONENT_H__

#include "Runtime/Math/Transform.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Framework/Object/GameObject.h"


namespace DSM {
    
    class TransformComponent : public Math::Transform, public IComponent
    {
    public:
        TransformComponent(std::shared_ptr<GameObject> gameObject)
            : IComponent(gameObject) {}
        TransformComponent(std::shared_ptr<GameObject> gameObject, Math::Vector3 pos, Math::Vector3 scale, Math::Quaternion rot) noexcept
            : IComponent(gameObject), m_Position(pos), m_Scale(scale), m_Rotation(rot) {}
        TransformComponent(std::shared_ptr<GameObject> gameObject, Math::Matrix4 matrix)
            : IComponent(gameObject)
        {
            m_Position = Math::GetPositionFromMatrix(matrix);
            m_Scale = Math::GetScaleFromMatrix(matrix);
            m_Rotation = Math::GetRotationFromMatrix(matrix);
        }
        TransformComponent(const TransformComponent&) = default;
        TransformComponent& operator=(const TransformComponent&) = default;
        TransformComponent(TransformComponent&&) = default;
        TransformComponent& operator=(TransformComponent&&) = default;
        virtual ~TransformComponent() = default;

        inline const Math::Vector3& GetPosition() const noexcept { return m_Position; }
        inline const Math::Vector3& GetScale() const noexcept { return m_Scale; }
        inline const Math::Quaternion& GetRotation() const noexcept { return m_Rotation; }

        inline Math::Vector3 GetRightAxis() const noexcept { return Math::Matrix3::GetRotate(m_Rotation).Get(0); }
        inline Math::Vector3 GetUpAxis() const noexcept { return Math::Matrix3::GetRotate(m_Rotation).Get(1); }
        inline Math::Vector3 GetForwardAxis() const noexcept { return Math::Matrix3::GetRotate(m_Rotation).Get(2); }

        inline Math::Matrix4 GetLocalToWorld() const noexcept { return DSM::Math::GetLocalToWorld(m_Position, m_Scale, m_Rotation); }
        inline Math::Matrix4 GetWorldToLocal() const noexcept { return Math::Matrix4::Inverse(GetLocalToWorld());}

        inline void SetPosition(Math::Vector3 pos) noexcept { m_Position = std::move(pos); m_IsDirty = true; }
        inline void SetPosition(float x, float y, float z) noexcept { m_Position = Math::Vector3{x, y, z}; m_IsDirty = true; }
        inline void SetScale(Math::Vector3 scale) noexcept { m_Scale = std::move(scale); m_IsDirty = true; }
        inline void SetScale(float x, float y, float z) noexcept { m_Scale = Math::Vector3{x, y, z}; m_IsDirty = true; }
        inline void SetRotation(Math::Quaternion rot) noexcept { m_Rotation = std::move(rot); m_IsDirty = true; }
        inline void SetRotation(float pitch, float yaw, float roll) noexcept 
        { 
            m_Rotation = Math::Quaternion{pitch, yaw, roll}; 
            m_IsDirty = true; 
        }

        inline void Translate(Math::Vector3 translation) noexcept { m_Position += translation; m_IsDirty = true; }
        inline void Rotate(const Math::Vector3& axis, float angle) noexcept 
        { 
            m_Rotation *= Math::Quaternion{axis, angle}; 
            m_IsDirty = true; 
        }
        // 根据 俯仰角、偏航角、滚动角 进行旋转
        inline void Rotate(const Math::Vector3& pyr) { m_Rotation = Math::Rotate(m_Rotation, pyr); m_IsDirty = true; }
        inline void Rotate(float pitch, float yaw, float roll) noexcept { Rotate(Math::Vector3{pitch, yaw, roll}); m_IsDirty = true; }
        // 绕特定的点进行旋转
        void Rotate(Math::Vector3 point, Math::Vector3 axis, float angle) noexcept
        {
            Math::Rotate(m_Rotation, m_Position, point, axis, angle);
            m_IsDirty = true;
        }
        
        void LookAt(const Math::Vector3& target, Math::Vector3 up = Math::Vector3{0, 1, 0}) noexcept
        {
            m_Rotation = Math::LookAt(m_Position, target, up);
            m_IsDirty = true;
        }
        void LookTo(const Math::Vector3& dir, Math::Vector3 up = Math::Vector3{0, 1, 0}) noexcept
        {
            m_Rotation = Math::LookTo(m_Position, dir, up);
            m_IsDirty = true;
        }

    private:
        Math::Vector3 m_Position{};
        Math::Vector3 m_Scale = Math::Vector3{1,1,1};
        Math::Quaternion m_Rotation;
    };

}

#endif
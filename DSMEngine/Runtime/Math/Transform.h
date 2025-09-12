#pragma once
#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include "MathCommon.h"


namespace DSM::Math {
    
    class Transform
    {
    public:
        Transform() noexcept = default;
        Transform(Math::Vector3 pos, Math::Vector3 scale, Math::Quaternion rot) noexcept
            :m_Position(pos), m_Scale(scale), m_Rotation(rot){}

        inline const Math::Vector3& GetPosition() const noexcept { return m_Position; }
        inline const Math::Vector3& GetScale() const noexcept { return m_Scale; }
        inline const Math::Quaternion& GetRotation() const noexcept { return m_Rotation; }

        inline Math::Vector3 GetRightAxis() const noexcept { return Math::Matrix3::GetRotate(m_Rotation).Get(0); }
        inline Math::Vector3 GetUpAxis() const noexcept { return Math::Matrix3::GetRotate(m_Rotation).Get(1); }
        inline Math::Vector3 GetForwardAxis() const noexcept { return Math::Matrix3::GetRotate(m_Rotation).Get(2); }

        inline Math::Matrix4 GetLocalToWorld() const noexcept { return DSM::Math::GetLocalToWorld(m_Position, m_Scale, m_Rotation); }
        inline Math::Matrix4 GetWorldToLocal() const noexcept { return Math::Matrix4::Inverse(GetLocalToWorld());}

        inline void SetPosition(Math::Vector3 pos) noexcept { m_Position = std::move(pos); }
        inline void SetPosition(float x, float y, float z) noexcept { m_Position = Math::Vector3{x, y, z}; }
        inline void SetScale(Math::Vector3 scale) noexcept { m_Scale = std::move(scale); }
        inline void SetScale(float x, float y, float z) noexcept { m_Scale = Math::Vector3{x, y, z}; }
        inline void SetRotation(Math::Quaternion rot) noexcept { m_Rotation = std::move(rot); }
        inline void SetRotation(float pitch, float yaw, float roll) noexcept { m_Rotation = Math::Quaternion{pitch, yaw, roll}; }

        inline void Translate(Math::Vector3 translation) noexcept { m_Position += translation; }
        inline void Rotate(const Math::Vector3& axis, float angle) noexcept { m_Rotation *= Math::Quaternion{axis, angle}; }
        // 根据 俯仰角、偏航角、滚动角 进行旋转
        inline void Rotate(float pitch, float yaw, float roll) noexcept 
        {
            Math::Vector3 angles = m_Rotation.ToEulerAngles();
            angles += Math::Vector3{pitch, yaw, roll};
            m_Rotation = Math::Quaternion{angles};
        }
        // 根据 俯仰角、偏航角、滚动角 进行旋转
        inline void Rotate(Math::Vector3 pyr) { m_Rotation *= Math::Quaternion{pyr.Get(0), pyr.Get(1), pyr.Get(2)}; }
        // 绕特定的点进行旋转
        void Rotate(Math::Vector3 point, Math::Vector3 axis, float angle) noexcept
        {
            // 计算新的旋转
            Math::Quaternion rotate{axis, angle};
            m_Rotation = rotate * m_Rotation;
            // 计算新的位置
            // 先将向量旋转
            Math::Vector3 rotateRelativePos = rotate * (m_Position - point);
            m_Position = point + rotateRelativePos;
        }
        
        void LookAt(const Math::Vector3& target, Math::Vector3 up = Math::Vector3{0, 1, 0}) noexcept
        {
            m_Rotation = Math::LookAt(m_Position, target, up);
        }
        void LookTo(const Math::Vector3& dir, Math::Vector3 up = Math::Vector3{0, 1, 0}) noexcept
        {
            m_Rotation = Math::LookTo(m_Position, dir, up);
        }

    private:
        Math::Vector3 m_Position{};
        Math::Vector3 m_Scale = Math::Vector3{1,1,1};
        Math::Quaternion m_Rotation;
    };

}

#endif
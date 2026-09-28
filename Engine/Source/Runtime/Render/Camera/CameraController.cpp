#include "CameraController.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/Core/Macro.h"

namespace DSM {

    void CameraController::Update(float deltaTime)
    {
        assert(m_pCamera != nullptr);

        const auto inputSystem = DSMEngine::sm_GlobalContext.inputSystem;
        const CameraInputState input = inputSystem != nullptr
            ? inputSystem->SampleCameraInput()
            : CameraInputState{};
        const float yaw = input.rotate ? input.mouseDeltaX * m_MouseSensitivityX : 0.0f;
        const float pitch = input.rotate ? input.mouseDeltaY * m_MouseSensitivityY : 0.0f;
        const int forward = input.forward;
        const int strafe = input.strafe;
        if (forward || strafe) {
            m_MoveDir = m_pCamera->GetLookAxis() * static_cast<float>(forward) +
                m_pCamera->GetRightAxis() * static_cast<float>(strafe);
            m_MoveVelocity = m_MoveSpeed;
            m_DragTimer = m_TotalDragTimeToZero;
            m_VelocityDrag = m_MoveSpeed / m_DragTimer;
        }
        else if (m_DragTimer > 0.0f) {
            m_DragTimer -= deltaTime;
            m_MoveVelocity = std::max(0.0f, m_MoveVelocity - m_VelocityDrag * deltaTime);
        }
        else {
            m_MoveVelocity = 0.0f;
        }

        Math::Vector3 euler = m_pCamera->GetRotation().ToEulerAngles();
        euler += Math::Vector3{pitch, yaw, 0.0f};
        const float piHalf = std::numbers::pi * 0.49f;
        euler.Set(0, std::clamp(float(euler.Get(0)), -piHalf, piHalf));

        if (euler.Get(1) > float(std::numbers::pi)) {
            euler.Set(1, euler.Get(1) - float(std::numbers::pi) * 2);
        }
        else if (euler.Get(1) <= -float(std::numbers::pi)) {
            euler.Set(1, euler.Get(1) + float(std::numbers::pi) * 2);
        }
        euler.Set(2, 0);

        m_pCamera->SetRotation(euler);
        m_pCamera->Translate(m_MoveDir * m_MoveVelocity * deltaTime);
    }

    void CameraController::InitCamera(Camera* pCamera)
    {
        m_pCamera = pCamera;
    }

    void CameraController::SetMouseSensitivity(float x, float y)
    {
        m_MouseSensitivityX = x;
        m_MouseSensitivityY = y;
    }

    void CameraController::SetMoveSpeed(float speed)
    {
        m_MoveSpeed = speed;
    }

} // namespace DSM

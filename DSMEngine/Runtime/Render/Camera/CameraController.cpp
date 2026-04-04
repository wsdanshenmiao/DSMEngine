#include "CameraController.h"
#include <imgui.h>
#include "Runtime/Core/Macro.h"

namespace DSM {

    void CameraController::Update(float deltaTime)
    {
        assert(m_pCamera != nullptr);

        ImGuiIO& io = ImGui::GetIO();

        float yaw = 0.0f, pitch = 0.0f;
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {

            yaw += io.MouseDelta.x * m_MouseSensitivityX;
            pitch += io.MouseDelta.y * m_MouseSensitivityY;
        }

        int forward = (
            (ImGui::IsKeyDown(ImGuiKey_W) ? 1 : 0) +
            (ImGui::IsKeyDown(ImGuiKey_S) ? -1 : 0)
            );
        int strafe = (
            (ImGui::IsKeyDown(ImGuiKey_A) ? -1 : 0) +
            (ImGui::IsKeyDown(ImGuiKey_D) ? 1 : 0)
            );

        if (forward || strafe) {
            m_MoveDir = m_pCamera->GetLookAxis() * (float)forward + m_pCamera->GetRightAxis() * (float)strafe;
            m_MoveVelocity = m_MoveSpeed;
            m_DragTimer = m_TotalDragTimeToZero;
            m_VelocityDrag = m_MoveSpeed / m_DragTimer;
        }
        else {
            if (m_DragTimer > 0.0f) {
                m_DragTimer -= deltaTime;
                m_MoveVelocity -= m_VelocityDrag * deltaTime;
            }
            else {
                m_MoveVelocity = 0.0f;
            }
        }

        Math::Vector3 euler = m_pCamera->GetRotation().ToEulerAngles();
        euler += Math::Vector3{pitch, yaw, 0.0f};
        float pidiv2 = std::numbers::pi * 0.49f;
        euler.Set(0, std::min(pidiv2, float(euler.Get(0))));
        euler.Set(0, std::max(-pidiv2, float(euler.Get(0))));

        if(euler.Get(1) > float(std::numbers::pi))
            euler.Set(1, euler.Get(1) - float(std::numbers::pi) * 2);
        else if(euler.Get(1) <= -float(std::numbers::pi))
            euler.Set(1, euler.Get(1) + float(std::numbers::pi) * 2);
        euler.Set(2, 0);

        m_pCamera->SetRotation(euler);
        m_pCamera->Translate(m_MoveDir * m_MoveVelocity * deltaTime);
    }

    void CameraController::InitCamera(DSM::Camera* pCamera)
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

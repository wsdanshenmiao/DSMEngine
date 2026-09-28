#include "InputSystem.h"
#include "Runtime/Core/Window.h"
#include <GLFW/glfw3.h>

namespace DSM {
    InputSystem::InputSystem(Window* window)
        : m_Window(window) {}

    bool InputSystem::IsKeyPressed(KeyCode keycode)
    {
        return glfwGetKey(m_Window->GetNativeWindow(), static_cast<int>(keycode)) == GLFW_PRESS;
    }

    bool InputSystem::IsMouseButtonPressed(MouseCode mouseCode)
    {
        return glfwGetMouseButton(m_Window->GetNativeWindow(), static_cast<int>(mouseCode)) == GLFW_PRESS;
    }

    Math::Vector2 InputSystem::GetMousePosition()
    {
        double xpos = 0.0, ypos = 0.0;
        glfwGetCursorPos(m_Window->GetNativeWindow(), &xpos, &ypos);
        return Math::Vector2{static_cast<float>(xpos), static_cast<float>(ypos)};
    }

    float InputSystem::GetMouseX()
    {
        return static_cast<float>(GetMousePosition().Get(0));
    }

    float InputSystem::GetMouseY()
    {
        return static_cast<float>(GetMousePosition().Get(1));
    }

    CameraInputState InputSystem::SampleCameraInput()
    {
        if (m_CameraInputOverride) {
            return *m_CameraInputOverride;
        }

        const auto mousePosition = GetMousePosition();
        if (!m_HasMousePosition) {
            m_LastMousePosition = mousePosition;
            m_HasMousePosition = true;
        }
        const auto mouseDelta = mousePosition - m_LastMousePosition;
        m_LastMousePosition = mousePosition;

        CameraInputState state{};
        state.forward = static_cast<int>(IsKeyPressed(KeyCode::W)) - static_cast<int>(IsKeyPressed(KeyCode::S));
        state.strafe = static_cast<int>(IsKeyPressed(KeyCode::D)) - static_cast<int>(IsKeyPressed(KeyCode::A));
        state.rotate = IsMouseButtonPressed(MouseCode::ButtonRight);
        state.mouseDeltaX = static_cast<float>(mouseDelta.Get(0));
        state.mouseDeltaY = static_cast<float>(mouseDelta.Get(1));
        return state;
    }

    void InputSystem::SetCameraInputOverride(const CameraInputState& state)
    {
        m_CameraInputOverride = state;
    }

    void InputSystem::ClearCameraInputOverride()
    {
        m_CameraInputOverride.reset();
        m_HasMousePosition = false;
    }
}

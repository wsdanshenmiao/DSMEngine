#include "InputSystem.h"
#include "Runtime/Core/Window.h"
#include <GLFW/glfw3.h>

namespace DSM {
    InputSystem::InputSystem(Window *window)
        :m_Window(window) {}

    bool InputSystem::IsKeyPressed(KeyCode keycode)
    {
        int state = glfwGetKey(m_Window->GetNativeWindow(), static_cast<int>(keycode));
        return state == GLFW_PRESS;
    }

    bool InputSystem::IsMouseButtonPressed(MouseCode mouseCode)
    {
        int state = glfwGetMouseButton(m_Window->GetNativeWindow(), static_cast<int>(mouseCode));
        return state == GLFW_PRESS;
    }

    Math::Vector2 InputSystem::GetMousePosition()
    {
		double xpos, ypos;
		glfwGetCursorPos(m_Window->GetNativeWindow(), &xpos, &ypos);

		return Math::Vector2{ (float)xpos, (float)ypos };    
    }

    float InputSystem::GetMouseX()
    {
        return GetMousePosition().Get(0);
    }

    float InputSystem::GetMouseY()
    {
        return GetMousePosition().Get(1);
    }
}
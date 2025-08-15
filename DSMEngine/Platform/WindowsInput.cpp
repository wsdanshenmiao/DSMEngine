#include "Core/Input.h"
#include "Core/Application.h"
#include <glfw3.h>

namespace DSM::Input {
    bool IsKeyPressed(KeyCode keycode)
    {
        auto window = static_cast<GLFWwindow*>(Application::GetInstance().GetWindow().GetNativeWindow());
        int state = glfwGetKey(window, static_cast<int>(keycode));
        return state == GLFW_PRESS;
    }

    bool IsMouseButtonPressed(MouseCode mouseCode)
    {
        auto window = static_cast<GLFWwindow*>(Application::GetInstance().GetWindow().GetNativeWindow());
        int state = glfwGetMouseButton(window, static_cast<int>(mouseCode));
        return state == GLFW_PRESS;
    }

    Vector2f GetMousePosition()
    {
		auto* window = static_cast<GLFWwindow*>(Application::GetInstance().GetWindow().GetNativeWindow());
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		return Vector2f{ (float)xpos, (float)ypos };    
    }

    float GetMouseX()
    {
        return GetMousePosition()[0];
    }

    float GetMouseY()
    {
        return GetMousePosition()[1];
    }

} // namespace DSM


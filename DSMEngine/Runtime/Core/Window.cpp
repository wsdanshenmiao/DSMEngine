#include "Runtime/Core/Window.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Event/KeyEvent.h"
#include "Runtime/Event/MouseButtonEvent.h"
#include <GLFW/glfw3.h>

namespace DSM {
    static uint8_t s_GLFWWindowCount = 0;

    Window::Window(const WindowProps &winProps)
    {
        m_Desc.title = winProps.m_Title;
        m_Desc.width = winProps.m_Width;
        m_Desc.height = winProps.m_Height;

        if(s_GLFWWindowCount == 0){
            DSM_CORE_ASSERT(glfwInit() != 0, "Failed to initialize GLFW!");
            glfwSetErrorCallback([](int error, const char* description){
                DSM_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
            });
        }

        
        m_Window = glfwCreateWindow(m_Desc.width, m_Desc.height, m_Desc.title.c_str(), nullptr, nullptr);
        DSM_CORE_ASSERT(m_Window != nullptr, "Failed to create GLFW window!");
        s_GLFWWindowCount++;
		glfwMakeContextCurrent(m_Window);

        glfwSetWindowUserPointer(m_Window, &m_Desc);
        
        SetVSync(true);

        // Set call back
        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window){
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if(data == nullptr || data->callback == nullptr)
                return;
            WindowCloseEvent event{};
            data->callback(event);
        });
        
        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height){
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if(data == nullptr || data->callback == nullptr)
                return;
            data->width = width;
            data->height = height;
            WindowResizeEvent event{(uint32_t)width, (uint32_t)height};
            data->callback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods){
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if(data == nullptr || data->callback == nullptr)
                return;

			switch (action){
            case GLFW_PRESS: {
                KeyPressedEvent event{(KeyCode)key};
                data->callback(event);
                break;
            }
            case GLFW_RELEASE:{
                KeyReleasedEvent event{(KeyCode)key};
                data->callback(event);
                break;
            }
            case GLFW_REPEAT:{
                KeyPressedEvent event{(KeyCode)key, true};
                data->callback(event);
                break;
            }
			}
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if(data == nullptr || data->callback == nullptr)
                return;

			switch (action){
            case GLFW_PRESS:{
                MouseButtonPressedEvent event{(MouseCode)button};
                data->callback(event);
                break;
            }
            case GLFW_RELEASE:{
                MouseButtonReleasedEvent event{(MouseCode)button};
                data->callback(event);
                break;
            }
			}
		});

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double x, double y)
		{
			auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if(data == nullptr || data->callback == nullptr)
                return;

			MouseMovedEvent event((float)x, (float)y);
			data->callback(event);
		});

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if(data == nullptr || data->callback == nullptr)
                return;

			MouseMovedEvent event((float)xOffset, (float)yOffset);
			data->callback(event);
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
		{
			auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if(data == nullptr || data->callback == nullptr)
                return;

			KeyTypedEvent event((KeyCode)keycode);
			data->callback(event);
		});
    }

    Window::~Window()
    {
        glfwDestroyWindow(m_Window);
        s_GLFWWindowCount--;

        // 销毁所有资源
        if(s_GLFWWindowCount == 0){
            glfwTerminate();
        }
    }

    void Window::Update()
    {
        // 处理事件
        glfwPollEvents();
    }

    void Window::SetTitle(const std::string &title)
    {
        m_Desc.title = title;
        glfwSetWindowTitle(m_Window, m_Desc.title.c_str());
    }

    void Window::SetVSync(bool enabled)
    {
        m_Desc.VSync = enabled;
        if(enabled){
            glfwSwapInterval(1);
        }
        else{
            glfwSwapInterval(0);
        }
    }


} // namespace DSM 
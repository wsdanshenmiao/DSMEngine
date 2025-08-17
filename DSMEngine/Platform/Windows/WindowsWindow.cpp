#include "WindowsWindow.h"
#include "Core/Core.h"
#include "Utils/Utils.h"
#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "Event/MouseButtonEvent.h"
#include <GLFW/glfw3.h>


namespace DSM { 
    static uint8_t s_GLFWWindowCount = 0;

    WindowsWindow::WindowsWindow(const WindowProps &winProps)
    {
        m_Desc.title = winProps.m_Title;
        m_Desc.width = winProps.m_Width;
        m_Desc.height = winProps.m_Height;

        DSM_CORE_INFO("Creating window {} ({}, {})", m_Desc.title, m_Desc.width, m_Desc.height);

        if(s_GLFWWindowCount == 0){
            int success = glfwInit();
            DSM_CORE_ASSERT(success, "Could not initialize glfw.");
            glfwSetErrorCallback([](int error, const char* description){
                DSM_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
            });
        }

        
        m_Window = glfwCreateWindow(m_Desc.width, m_Desc.height, m_Desc.title.c_str(), nullptr, nullptr);
        s_GLFWWindowCount--;
		glfwMakeContextCurrent(m_Window);

        glfwSetWindowUserPointer(m_Window, &m_Desc);
        
        SetVSync(true);

        // Set call back
        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window){
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            WindowCloseEvent event{};
            data->callback(event);
        });
        
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height){
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data->width = width;
            data->height = height;
            WindowResizeEvent event{(uint32_t)width, (uint32_t)height};
            data->callback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods){
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

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

			MouseMovedEvent event((float)x, (float)y);
			data->callback(event);
		});

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			MouseMovedEvent event((float)xOffset, (float)yOffset);
			data->callback(event);
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
		{
			auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			KeyTypedEvent event((KeyCode)keycode);
			data->callback(event);
		});
    }

    WindowsWindow::~WindowsWindow()
    {
        glfwDestroyWindow(m_Window);
        s_GLFWWindowCount--;

        // 销毁所有资源
        if(s_GLFWWindowCount == 0){
            glfwTerminate();
        }
    }

    void WindowsWindow::OnUpdate()
    {
        // 处理事件
        glfwPollEvents();
        glfwSwapBuffers(m_Window);
    }

    void WindowsWindow::SetVSync(bool enabled)
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
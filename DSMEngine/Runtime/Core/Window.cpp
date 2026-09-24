#include "Runtime/Core/Window.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Event/KeyEvent.h"
#include "Runtime/Event/MouseButtonEvent.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <stdexcept>

namespace DSM {
    static uint32_t s_GLFWWindowCount = 0;

    Window::Window(const WindowProps& winProps)
    {
        m_Desc.title = winProps.m_Title;
        m_Desc.width = winProps.m_Width;
        m_Desc.height = winProps.m_Height;

        const bool initializeGLFW = s_GLFWWindowCount == 0;
        if (initializeGLFW) {
            glfwSetErrorCallback([](int error, const char* description) {
                if (DSMEngine::sm_GlobalContext.loggerSystem != nullptr) {
                    DSM_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
                }
                else {
                    std::fprintf(stderr, "GLFW Error (%d): %s\n", error, description != nullptr ? description : "unknown");
                }
            });

            if (glfwInit() != GLFW_TRUE) {
                glfwSetErrorCallback(nullptr);
                throw std::runtime_error("Failed to initialize GLFW");
            }
        }

        m_Window = glfwCreateWindow(
            static_cast<int>(m_Desc.width),
            static_cast<int>(m_Desc.height),
            m_Desc.title.c_str(),
            nullptr,
            nullptr);
        if (m_Window == nullptr) {
            if (initializeGLFW) {
                glfwTerminate();
                glfwSetErrorCallback(nullptr);
            }
            throw std::runtime_error("Failed to create GLFW window");
        }

        ++s_GLFWWindowCount;
        glfwMakeContextCurrent(m_Window);

        glfwSetWindowUserPointer(m_Window, &m_Desc);
        SetVSync(true);

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data == nullptr || data->callback == nullptr)
                return;
            WindowCloseEvent event{};
            data->callback(event);
        });

        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data == nullptr || data->callback == nullptr)
                return;

            const uint32_t safeWidth = width > 0 ? static_cast<uint32_t>(width) : 0u;
            const uint32_t safeHeight = height > 0 ? static_cast<uint32_t>(height) : 0u;
            data->width = safeWidth;
            data->height = safeHeight;
            WindowResizeEvent event{safeWidth, safeHeight};
            data->callback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int, int action, int) {
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data == nullptr || data->callback == nullptr)
                return;

            switch (action) {
            case GLFW_PRESS: {
                KeyPressedEvent event{static_cast<KeyCode>(key)};
                data->callback(event);
                break;
            }
            case GLFW_RELEASE: {
                KeyReleasedEvent event{static_cast<KeyCode>(key)};
                data->callback(event);
                break;
            }
            case GLFW_REPEAT: {
                KeyPressedEvent event{static_cast<KeyCode>(key), true};
                data->callback(event);
                break;
            }
            default:
                break;
            }
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int) {
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data == nullptr || data->callback == nullptr)
                return;

            switch (action) {
            case GLFW_PRESS: {
                MouseButtonPressedEvent event{static_cast<MouseCode>(button)};
                data->callback(event);
                break;
            }
            case GLFW_RELEASE: {
                MouseButtonReleasedEvent event{static_cast<MouseCode>(button)};
                data->callback(event);
                break;
            }
            default:
                break;
            }
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double x, double y) {
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data == nullptr || data->callback == nullptr)
                return;

            MouseMovedEvent event{static_cast<float>(x), static_cast<float>(y)};
            data->callback(event);
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset) {
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data == nullptr || data->callback == nullptr)
                return;

            MouseScrolledEvent event{static_cast<float>(xOffset), static_cast<float>(yOffset)};
            data->callback(event);
        });

        glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode) {
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data == nullptr || data->callback == nullptr)
                return;

            KeyTypedEvent event{static_cast<KeyCode>(keycode)};
            data->callback(event);
        });

        glfwSetDropCallback(m_Window, [](GLFWwindow* window, int count, const char** paths) {
            auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            if (data == nullptr || paths == nullptr || count <= 0)
                return;

            for (int i = 0; i < count; ++i) {
                if (paths[i] != nullptr) {
                    data->droppedPaths.emplace_back(paths[i]);
                }
            }
        });
    }

    Window::~Window()
    {
        if (m_Window == nullptr)
            return;

        glfwDestroyWindow(m_Window);
        m_Window = nullptr;

        if (s_GLFWWindowCount > 0) {
            --s_GLFWWindowCount;
        }

        if (s_GLFWWindowCount == 0) {
            glfwTerminate();
            glfwSetErrorCallback(nullptr);
        }
    }

    void Window::Update()
    {
        if (m_Window != nullptr) {
            glfwPollEvents();
        }
    }

    void Window::SetTitle(const std::string& title)
    {
        m_Desc.title = title;
        if (m_Window != nullptr) {
            glfwSetWindowTitle(m_Window, m_Desc.title.c_str());
        }
    }

    void Window::SetVSync(bool enabled)
    {
        m_Desc.VSync = enabled;
        if (m_Window == nullptr)
            return;

        glfwSwapInterval(enabled ? 1 : 0);
    }

    bool Window::IsMinimized() const
    {
        return m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED) == GLFW_TRUE;
    }

    bool Window::IsFullScreen() const
    {
        return m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) == GLFW_TRUE;
    }

    std::vector<std::filesystem::path> Window::ConsumeDroppedPaths()
    {
        auto result = std::move(m_Desc.droppedPaths);
        m_Desc.droppedPaths.clear();
        return result;
    }

} // namespace DSM

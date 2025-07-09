#include "WindowsWindow.h"
#include "Core/Core.h"
#include "Event/ApplicationEvent.h"

namespace DSM { 
    WindowsWindow::WindowsWindow(const WindowProps &winProps)
    {
        std::wstring title = Utility::UTF8ToWString(winProps.m_Title);
        m_Width = winProps.m_Width;
        m_Height = winProps.m_Height;

        HINSTANCE hInstance = GetModuleHandle(nullptr);

        // 回调函数
        auto WndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) ->LRESULT {
            auto* pWindow = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            std::unique_ptr<Event> pEvent{};
            switch( message ) {
            case WM_SIZE: {
                auto width = (UINT)(UINT64)lParam & 0xFFFF;
                auto height = (UINT)(UINT64)lParam >> 16;
                pEvent = std::make_unique<WindowResizeEvent>(width, height);
                break;
            }
            case WM_DESTROY: {
                pEvent = std::make_unique<WindowCloseEvent>();
                PostQuitMessage(0); 
                break;
            }
            default:
                return DefWindowProc( hWnd, message, wParam, lParam );
            }
            if(pEvent != nullptr && pWindow->m_Callback != nullptr){
                pWindow->m_Callback(*pEvent);
            }

            return 0;
        };

        // 注册类
        WNDCLASSEX wcex{};
        wcex.hInstance = hInstance;
        wcex.lpszClassName = title.c_str();
        wcex.style= CS_HREDRAW | CS_VREDRAW;
        wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpfnWndProc = WndProc;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);
        wcex.lpszMenuName = nullptr;
        DSM_ASSERT(0 != RegisterClassEx(&wcex), "RegisterClassEx failed");

        // 创建窗口
        RECT rect{ 0, 0, (LONG)winProps.m_Width, (LONG)winProps.m_Height };
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        m_Handle = CreateWindow(
            title.c_str(),
            title.c_str(),
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr, nullptr,
            hInstance, nullptr);

        // 设置当前窗口关联的指针
        SetWindowLongPtr(m_Handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        ShowWindow(m_Handle, SW_SHOWNORMAL);
    }

    void WindowsWindow::OnUpdate()
    {
        UpdateWindow(m_Handle);
        MSG msg{};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT) break;
        }
    }

    void *WindowsWindow::GetNativeWindow() const
    {
        return m_Handle;
    }

} // namespace DSM
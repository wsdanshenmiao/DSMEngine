#include "WindowsWindow.h"
#include "Core/Core.h"

namespace DSM { 
    LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

    WindowsWindow::WindowsWindow(const WindowProps &winProps)
    {
        auto windowsProps = dynamic_cast<const WindowsWindowProps&>(winProps);

        std::wstring title = Utility::UTF8ToWString(windowsProps.m_Title);
        m_Width = windowsProps.m_Width;
        m_Height = windowsProps.m_Height;

        // 注册类
        WNDCLASSEX wcex{};
        //wcex.hInstance = desc.m_HInstance;
        wcex.lpszClassName = title.c_str();
        wcex.style= CS_HREDRAW | CS_VREDRAW;
        wcex.hIcon = LoadIcon(windowsProps.m_hInstance, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpfnWndProc = WndProc;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hIconSm = LoadIcon(windowsProps.m_hInstance, IDI_APPLICATION);
        wcex.lpszMenuName = nullptr;
        DSM_ASSERT(0 != RegisterClassEx(&wcex), "RegisterClassEx failed");

        // 创建窗口
        RECT rect{ 0, 0, (LONG)windowsProps.m_Width, (LONG)windowsProps.m_Height };
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        m_Handle = CreateWindow(
            title.c_str(),
            title.c_str(),
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr, nullptr,
            windowsProps.m_hInstance, nullptr);

        ShowWindow(m_Handle, SW_SHOWNORMAL);


        UpdateWindow(m_Handle);
    }

    void WindowsWindow::OnUpdate()
    {
    }

    void *WindowsWindow::GetNativeWindow() const
    {
        return m_Handle;
    }


    LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
    {
        switch( message )
        {
        case WM_SIZE: {
            //GameCore::OnResize((UINT)(UINT64)lParam & 0xFFFF, (UINT)(UINT64)lParam >> 16); break;
        }
        case WM_DESTROY: {
            PostQuitMessage(0); break;
        }
        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
        }

        return 0;
    }

} // namespace DSM
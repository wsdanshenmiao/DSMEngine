#include "Runtime/Platform/PlatformUtils.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace DSM::Utility {
    std::wstring UTF8ToWString(const std::string& str) 
    {
        if (str.empty()) return L"";

        // 计算需要的宽字符数
        int wcharCount = MultiByteToWideChar(
            CP_UTF8,            // 源字符串是 UTF-8
            0,                  // 无特殊标志
            str.c_str(),        // 源字符串
            (int)str.size(),    // 字符串长度（-1 表示自动计算，但这里手动传入大小）
            nullptr,            // 不接收输出，仅计算缓冲区大小
            0                   // 输出缓冲区大小（0 表示仅计算）
        );

        if (wcharCount == 0) {
            return L"";
        }

        std::wstring wstr(wcharCount, 0);

        // 实际转换
        MultiByteToWideChar(
            CP_UTF8,
            0,
            str.c_str(),
            (int)str.size(),
            &wstr[0],          // 输出缓冲区
            wcharCount
        );

        return wstr;
    }

    std::string WStringToUTF8(const std::wstring& wstr)
    {
        if (wstr.empty()) return "";

        // 计算需要的字节数
        int byteCount = WideCharToMultiByte(
            CP_UTF8,            // 目标编码是 UTF-8
            0,                  // 无特殊标志
            wstr.c_str(),       // 源宽字符串
            (int)wstr.size(),   // 宽字符数
            nullptr,            // 不接收输出，仅计算缓冲区大小
            0,                  // 输出缓冲区大小（0 表示仅计算）
            nullptr,            // 默认字符（不可转换时使用）
            nullptr             // 是否使用了默认字符
        );

        if (byteCount == 0) {
            return "";
        }

        // 分配足够的空间（+1 用于 null 终止符）
        std::string str(byteCount, 0);

        // 实际转换
        WideCharToMultiByte(
            CP_UTF8,
            0,
            wstr.c_str(),
            (int)wstr.size(),
            &str[0],            // 输出缓冲区
            byteCount,
            nullptr,
            nullptr
        );

        return str;
    }

    std::string DSM::Utility::FileDialogs::OpenFile(const char *filter)
    {
        OPENFILENAMEA ofn;       // common dialog box structure
        CHAR szFile[260] = { 0 };       // buffer for file name
		CHAR currentDir[256] = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = glfwGetWin32Window(glfwGetCurrentContext());
        ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

        return std::string();
    }

    std::string DSM::Utility::FileDialogs::SaveFile(const char *filter)
    {
        OPENFILENAMEA ofn;       // common dialog box structure
        CHAR szFile[260] = { 0 };       // buffer for file name
		CHAR currentDir[256] = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = glfwGetWin32Window(glfwGetCurrentContext());
        ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		// Sets the default extension by extracting it from the filter
		ofn.lpstrDefExt = strchr(filter, '\0') + 1;

		if (GetSaveFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;
		
		return std::string();
    }

}
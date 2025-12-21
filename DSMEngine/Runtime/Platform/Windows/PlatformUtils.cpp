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

    std::vector<std::string> FileDialogs::OpenFile(
        const std::vector<FilterOption> &filters, 
        const std::string &title)
    {
        return ShowFileDialog(true, filters, title);
    }

    std::vector<std::string> FileDialogs::SaveFile(
        const std::vector<FilterOption> &filters, 
        const std::string &title)
    {
        return ShowFileDialog(false, filters, title);
    }

    std::vector<std::string> FileDialogs::ShowFileDialog(
        bool isOpenDialog,
        const std::vector<FilterOption> &filters,
        const std::string &title)
    {
        std::string filter;
        if(filters.empty()){
            filter = "All Files\0*.*\0";
        }
        else{
            for(const auto& [name, pattern] : filters){
                filter += name + '\0' + pattern + '\0';
            }
        }
        filter += '\0';

        const size_t bufferSize = 65536;
        std::vector<char> szFile(bufferSize, 0);

        OPENFILENAMEA ofn;       // common dialog box structure
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = glfwGetWin32Window(glfwGetCurrentContext());
        ofn.lpstrFile = szFile.data();
		ofn.nMaxFile = (DWORD)szFile.size();
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if(isOpenDialog){
            ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;
        }
        else {
            ofn.Flags |= OFN_OVERWRITEPROMPT;
        }
		
        CHAR currentDir[MAX_PATH] = { 0 };
		if (GetCurrentDirectoryA(MAX_PATH, currentDir))
			ofn.lpstrInitialDir = currentDir;
        if(!isOpenDialog){
            // Sets the default extension by extracting it from the filter
            ofn.lpstrDefExt = strchr(filter.c_str(), '\0') + 1;
        }

        if(isOpenDialog ? GetOpenFileNameA(&ofn) : GetSaveFileNameA(&ofn)){
            return isOpenDialog ? ParseMultiSelectFiles(ofn.lpstrFile) : 
                std::vector<std::string>{ofn.lpstrFile};
        }
        return {};
    }
    
    std::vector<std::string> FileDialogs::ParseMultiSelectFiles(const char *buffer)
    {
        std::vector<std::string> files;
        
        if (buffer == nullptr || buffer[0] == '\0') {
            return files;
        }

        // 第一个字符串是目录路径
        std::string directory = buffer;
        
        // 指针移动到目录字符串之后
        const char* p = buffer + directory.length() + 1;
        
        if (p[0] == '\0') {
            // 如果第二个字符就是空，说明只选择了一个文件
            // 在这种情况下，directory 就是完整的文件路径
            files.push_back(directory);
        } else {
            // 多选情况：读取多个文件名
            while (p[0] != '\0') {
                std::string filename = p;
                std::string fullpath = directory + "\\" + filename;
                files.push_back(fullpath);
                p += filename.length() + 1;  // 移动到下一个字符串
            }
        }
        
        return files;
    }
}
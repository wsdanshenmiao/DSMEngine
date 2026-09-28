#include "Runtime/Platform/PlatformUtils.h"

#include <windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

namespace DSM::Utility {
    namespace {
        std::wstring BuildFilterString(const std::vector<FileDialogs::FilterOption>& filters)
        {
            std::wstring filter;
            if (filters.empty()) {
                filter.append(L"All Files");
                filter.push_back(L'\0');
                filter.append(L"*.*");
                filter.push_back(L'\0');
            }
            else {
                for (const auto& [name, pattern] : filters) {
                    filter.append(UTF8ToWString(name));
                    filter.push_back(L'\0');
                    filter.append(UTF8ToWString(pattern));
                    filter.push_back(L'\0');
                }
            }

            // OPENFILENAME 要求过滤器以两个连续的 NUL 结束。
            filter.push_back(L'\0');
            return filter;
        }

        std::wstring GetDefaultExtension(const std::vector<FileDialogs::FilterOption>& filters)
        {
            if (filters.empty()) {
                return {};
            }

            std::wstring pattern = UTF8ToWString(filters.front().pattern);
            const size_t separator = pattern.find_first_of(L";, ");
            if (separator != std::wstring::npos) {
                pattern.resize(separator);
            }

            const size_t dot = pattern.find_last_of(L'.');
            if (dot == std::wstring::npos || dot + 1 >= pattern.size()) {
                return {};
            }

            std::wstring extension = pattern.substr(dot + 1);
            if (extension.find(L'*') != std::wstring::npos || extension.find(L'?') != std::wstring::npos) {
                return {};
            }
            return extension;
        }
    }

    std::wstring UTF8ToWString(const std::string& str)
    {
        if (str.empty())
            return L"";

        const int sourceLength = static_cast<int>(str.size());
        const int wcharCount = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            str.data(),
            sourceLength,
            nullptr,
            0);
        if (wcharCount <= 0)
            return L"";

        std::wstring result(static_cast<size_t>(wcharCount), L'\0');
        if (MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                str.data(),
                sourceLength,
                result.data(),
                wcharCount) <= 0) {
            return L"";
        }
        return result;
    }

    std::string WStringToUTF8(const std::wstring& wstr)
    {
        if (wstr.empty())
            return "";

        const int sourceLength = static_cast<int>(wstr.size());
        const int byteCount = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            wstr.data(),
            sourceLength,
            nullptr,
            0,
            nullptr,
            nullptr);
        if (byteCount <= 0)
            return "";

        std::string result(static_cast<size_t>(byteCount), '\0');
        if (WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                wstr.data(),
                sourceLength,
                result.data(),
                byteCount,
                nullptr,
                nullptr) <= 0) {
            return "";
        }
        return result;
    }

    std::vector<std::string> FileDialogs::OpenFile(
        const std::vector<FilterOption>& filters,
        const std::string& title)
    {
        return ShowFileDialog(true, filters, title);
    }

    std::vector<std::string> FileDialogs::SaveFile(
        const std::vector<FilterOption>& filters,
        const std::string& title)
    {
        return ShowFileDialog(false, filters, title);
    }

    std::vector<std::string> FileDialogs::ShowFileDialog(
        bool isOpenDialog,
        const std::vector<FilterOption>& filters,
        const std::string& title)
    {
        const std::wstring filter = BuildFilterString(filters);
        const std::wstring titleW = UTF8ToWString(title);
        const std::wstring defaultExtension = GetDefaultExtension(filters);

        constexpr DWORD bufferSize = 65536;
        std::vector<wchar_t> fileBuffer(bufferSize, L'\0');
        std::vector<wchar_t> currentDirectory(MAX_PATH, L'\0');

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        if (GLFWwindow* context = glfwGetCurrentContext(); context != nullptr) {
            ofn.hwndOwner = glfwGetWin32Window(context);
        }
        ofn.lpstrFile = fileBuffer.data();
        ofn.nMaxFile = bufferSize;
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = titleW.empty() ? nullptr : titleW.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

        if (isOpenDialog) {
            ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;
        }
        else {
            ofn.Flags |= OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = defaultExtension.empty() ? nullptr : defaultExtension.c_str();
        }

        const DWORD currentDirectoryLength = GetCurrentDirectoryW(
            static_cast<DWORD>(currentDirectory.size()), currentDirectory.data());
        if (currentDirectoryLength > 0 && currentDirectoryLength < currentDirectory.size()) {
            ofn.lpstrInitialDir = currentDirectory.data();
        }

        const BOOL succeeded = isOpenDialog
            ? GetOpenFileNameW(&ofn)
            : GetSaveFileNameW(&ofn);
        if (!succeeded) {
            return {};
        }

        if (isOpenDialog) {
            return ParseMultiSelectFiles(fileBuffer.data());
        }
        return {WStringToUTF8(fileBuffer.data())};
    }

    std::vector<std::string> FileDialogs::ParseMultiSelectFiles(const wchar_t* buffer)
    {
        std::vector<std::string> files;
        if (buffer == nullptr || buffer[0] == L'\0') {
            return files;
        }

        const std::wstring directory = buffer;
        const wchar_t* current = buffer + directory.size() + 1;
        if (current[0] == L'\0') {
            // 单选时，第一个字符串就是完整路径。
            files.push_back(WStringToUTF8(directory));
            return files;
        }

        // OFN_EXPLORER 多选格式：目录、文件名1、文件名2、……、空字符串。
        while (current[0] != L'\0') {
            const std::wstring filename = current;
            const std::filesystem::path fullPath = std::filesystem::path(directory) / filename;
            files.push_back(WStringToUTF8(fullPath.wstring()));
            current += filename.size() + 1;
        }
        return files;
    }
}

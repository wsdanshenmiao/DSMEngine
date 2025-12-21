#ifndef __PLATFORM_UTILS_H__
#define __PLATFORM_UTILS_H__


#include "Runtime/Core/PlatformDetection.h"

#include <string>
#include <vector>

namespace DSM::Utility {
    
    std::wstring UTF8ToWString(const std::string& str);
    std::string WStringToUTF8(const std::wstring& wstr);

    class FileDialogs
    {
    public:
        struct FilterOption
        {
            std::string name;
            std::string pattern;
        };

    public:
        static std::vector<std::string> OpenFile(
            const std::vector<FilterOption>& filters, 
            const std::string& title = "Open File");
        static std::vector<std::string> SaveFile(
            const std::vector<FilterOption>& filters, 
            const std::string& title = "Save File");

    private:
        static std::vector<std::string> ShowFileDialog(
            bool isOpenDialog,
            const std::vector<FilterOption>& filters, 
            const std::string& title = "Save File");

        static std::vector<std::string> ParseMultiSelectFiles(const char* buffer);
    };
}


#endif
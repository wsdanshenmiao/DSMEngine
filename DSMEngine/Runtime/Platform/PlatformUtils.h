#ifndef __PLATFORM_UTILS_H__
#define __PLATFORM_UTILS_H__


#include "Runtime/Core/PlatformDetection.h"

#include <string>

namespace DSM::Utility {
    
    std::wstring UTF8ToWString(const std::string& str);
    std::string WStringToUTF8(const std::wstring& wstr);

    class FileDialogs
    {
    public:
        static std::string OpenFile(const char* filter);
        static std::string SaveFile(const char* filter);
    };
}


#endif
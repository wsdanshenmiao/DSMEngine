#pragma once
#ifndef __ASSERTDEFINE_H__
#define __ASSERTDEFINE_H__

#include <string>

namespace DSM {
    constexpr const char* g_SceneFileExtension = ".dsmscene";
    constexpr const char* g_ProjectFileExtension = ".dsmproj";
    constexpr const char* g_ContentBrowserDragDropPayload = "CONTENT_BROWSER_ITEM";
    
    // 项目的目录
    inline std::string g_ProjectFilePath = "";
}


#endif
#pragma once

#ifndef __CONTENT_BROWSER_PANEL_H__
#define __CONTENT_BROWSER_PANEL_H__

#include <filesystem>
#include "Runtime/Graphics/Texture.h"

namespace DSM {
    class ContentBrowserPanel
    {
    public:
        ContentBrowserPanel();

        void OnGUI();

    public:
        constexpr static const char* sm_DragDropPayloadType = "CONTENT_BROWSER_ITEM";

    private:
        std::filesystem::path m_RootDirectory;
        std::filesystem::path m_CurrDirectory;

        TextureHandle m_FolderIcon;
        TextureHandle m_FileIcon;
    };
}


#endif
#pragma once

#ifndef __CONTENT_BROWSER_PANEL_H__
#define __CONTENT_BROWSER_PANEL_H__

#include <filesystem>
#include "Runtime/Graphics/Texture.h"
#include "Editor/EditorUI/Widget.h"

namespace DSM {
    class EditorContentBrowser : public Widget
    {
    public:
        EditorContentBrowser(EditorUI* editorUI);

        void OnGUIEnabled() override;

    private:
        std::filesystem::path m_RootDirectory;
        std::filesystem::path m_CurrDirectory;

        TextureHandle m_FolderIcon;
        TextureHandle m_FileIcon;
    };
}


#endif
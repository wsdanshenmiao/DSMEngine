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
        void OnEvent(Event& event) override;

    private:
        std::filesystem::path MakeUniqueTargetPath(const std::filesystem::path& targetDir, const std::filesystem::path& srcPath);
        bool CopyPathToDirectory(const std::filesystem::path& srcPath, const std::filesystem::path& targetDir);

    private:
        std::filesystem::path m_RootDirectory{};
        std::filesystem::path m_CurrDirectory{};
        std::filesystem::path m_SelectedPath{};

        TextureHandle m_FolderIcon;
        TextureHandle m_FileIcon;
    };
}


#endif
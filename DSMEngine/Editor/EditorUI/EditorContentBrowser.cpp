#include "EditorContentBrowser.h"
#include "Editor/AssertDefine.h"
#include "Runtime/Render/TextureManager.h"
#include "Runtime/Core/Macro.h"

#include <imgui.h>

namespace DSM {
    EditorContentBrowser::EditorContentBrowser(EditorUI* editorUI)
        : Widget(editorUI),
        m_RootDirectory(std::filesystem::current_path()),
        m_CurrDirectory(m_RootDirectory)
    {
        m_Title = "Assets";
        m_Flags |= ImGuiWindowFlags_NoScrollbar;

        // 加载文件夹和文件图标
        m_FolderIcon = TextureManager::LoadTextureFromFile("Assets/Textures/Icons/ContentBrowser/DirectoryIcon.png");
        m_FileIcon = TextureManager::LoadTextureFromFile("Assets/Textures/Icons/ContentBrowser/FileIcon.png");
        DSM_CORE_ASSERT(m_FolderIcon != nullptr, "Failed to load folder icon texture!");
        DSM_CORE_ASSERT(m_FileIcon != nullptr, "Failed to load file icon texture!");
    }

    void EditorContentBrowser::OnGUIEnabled()
    {
        if(m_CurrDirectory.empty()){
            return;
        }

        if(m_CurrDirectory != m_RootDirectory){
            if(ImGui::Button("<-")){
                m_CurrDirectory = m_CurrDirectory.parent_path();
            }
        }

        //  各个文件和文件夹之间的间隔
		constexpr static float padding = 16.0f;
		constexpr static float thumbnailSize = 128.0f;
        constexpr static float cellSize = thumbnailSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = static_cast<int>(panelWidth / cellSize);
        columnCount = std::max(columnCount, 1);

        // 进行列布局
        ImGui::Columns(columnCount, 0, false);

        for(auto& directory : std::filesystem::directory_iterator(m_CurrDirectory)){
            const auto& path = directory.path();

            auto filename = path.filename().string();
            ImGui::PushID(filename.c_str());

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
            
            // 判断是否是文件夹，选择不同的图标
            ITexture* tex = directory.is_directory() ? m_FolderIcon.Get() : m_FileIcon.Get();
            // TODO: 后续需要改为各 API 相关的纹理句柄
            auto gpuHandle = tex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);
            ImGui::ImageButton(filename.c_str(), ImTextureRef{gpuHandle}, ImVec2(64, 64));

            // 拖拽源，可以拖拽文件到其他面板
            if(ImGui::BeginDragDropSource()){
                auto filepath = path.string();
                ImGui::SetDragDropPayload(g_ContentBrowserDragDropPayload, filepath.c_str(), filepath.size() + 1);
                ImGui::EndDragDropSource();
            }

            ImGui::PopStyleColor();

            // 双击进入文件夹
            if(directory.is_directory() && ImGui::IsItemHovered() && 
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
                m_CurrDirectory /= path.filename();
            }
            // 文件名自动换行
            ImGui::TextWrapped(filename.c_str());
            
            ImGui::NextColumn();
            ImGui::PopID();
        }

        // 恢复单列布局
        ImGui::Columns(1);
    }
}
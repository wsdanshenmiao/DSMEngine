#include "EditorContentBrowser.h"
#include "Editor/DSMEditor.h"
#include "Editor/Project.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Render/TextureManager.h"
#include "Runtime/Event/KeyEvent.h"

#include <imgui.h>

namespace DSM {
    EditorContentBrowser::EditorContentBrowser(EditorUI* editorUI)
        : Widget(editorUI)
    {
        m_Title = "Assets";
        m_Flags |= ImGuiWindowFlags_NoScrollbar;

        // 加载文件夹和文件图标
        m_FolderIcon = TextureManager::LoadTextureFromFile("Assets/Textures/Icons/ContentBrowser/DirectoryIcon.png");
        m_FileIcon = TextureManager::LoadTextureFromFile("Assets/Textures/Icons/ContentBrowser/FileIcon.png");
    }

    void EditorContentBrowser::OnGUIEnabled()
    {
        if (auto projPath = Project::GetInstance().GetFilePath(); !projPath.empty()) {
			auto path = std::filesystem::path(projPath);
            path = path.parent_path();
            if(!path.empty() && m_RootDirectory != path){
                m_RootDirectory = path;
                m_CurrDirectory = m_RootDirectory;
			}
        }

        if(m_FolderIcon == nullptr || m_FileIcon == nullptr){
            ImGui::TextUnformatted("Content Browser icons failed to load.");
            return;
        }

        if(m_CurrDirectory != m_RootDirectory){
            if(ImGui::Button("<-")){
                m_CurrDirectory = m_CurrDirectory.parent_path();
            }
        }

        // Windows 外部拖入（如资源管理器）: 当鼠标位于 ContentBrowser 窗口时，将路径复制到当前目录。
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)){
            if(auto window = DSMEngine::sm_GlobalContext.window; window != nullptr){
                auto droppedPaths = window->ConsumeDroppedPaths();
                for(const auto& droppedPath : droppedPaths){
                    CopyPathToDirectory(droppedPath, m_CurrDirectory);
                }
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
        bool anyItemHovered = false;

        for(auto& directory : std::filesystem::directory_iterator(m_CurrDirectory)){
            const auto& path = directory.path();
            const bool isSelected = (m_SelectedPath == path);

            auto filename = path.filename().string();
            ImGui::PushID(filename.c_str());
            
            // 判断是否是文件夹，选择不同的图标
            ITexture* tex = directory.is_directory() ? m_FolderIcon.Get() : m_FileIcon.Get();
            // TODO: 后续需要改为各 API 相关的纹理句柄
            auto gpuHandle = tex->GetNativeView(ObjectTypes::D3D12_ShaderResourceViewGpuDescriptor);

            // 选中项使用持续高亮样式，和左侧树节点的选中体验保持一致。
            if(isSelected){
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            }
            ImGui::ImageButton(filename.c_str(), ImTextureRef{gpuHandle}, ImVec2(64, 64));
            if(isSelected){
                ImGui::PopStyleColor(3);
            }

            // 拖拽源，可以拖拽文件到其他面板
            if(ImGui::BeginDragDropSource()){
                auto filepath = path.string();
                ImGui::SetDragDropPayload(Project::s_ContentBrowserDragDropPayload, filepath.c_str(), filepath.size() + 1);
                ImGui::EndDragDropSource();
            }

            const bool itemHovered = ImGui::IsItemHovered();
            anyItemHovered |= itemHovered;
            if(itemHovered){
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && directory.is_directory()) {
                    // 双击进入文件夹
                    m_CurrDirectory /= path.filename();
                }
                else if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
                    // 单击选择文件
                    m_SelectedPath = path;
                }
            }

            // 文件名自动换行
            if(isSelected){
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
            }
            ImGui::TextWrapped(filename.c_str());
            if(isSelected){
                ImGui::PopStyleColor();
            }
            
            ImGui::NextColumn();
            ImGui::PopID();
        }

        // 恢复单列布局
        ImGui::Columns(1);

        // 点击空白区域时清空选择，而不是受上一个控件状态影响。
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !anyItemHovered){
            m_SelectedPath.clear();
        }
    }

    void EditorContentBrowser::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e){
            switch (e.GetKeyCode()) {
            case KeyCode::Delete:{
                if(!m_SelectedPath.empty() && std::filesystem::exists(m_SelectedPath)){
                    std::error_code ec;
                    if(std::filesystem::is_directory(m_SelectedPath)){
                        std::filesystem::remove_all(m_SelectedPath, ec);
                    }
                    else{
                        std::filesystem::remove(m_SelectedPath, ec);
                    }
                    if(!ec){
                        m_SelectedPath.clear();
                    }
                }
                break;
            } 
            default:
                break;
            }
            return false;
        });
    }

    std::filesystem::path EditorContentBrowser::MakeUniqueTargetPath(const std::filesystem::path& targetDir, const std::filesystem::path& srcPath)
    {
        // 获取目标路径下与源文件同名的路径
        auto candidate = targetDir / srcPath.filename();
        if(!std::filesystem::exists(candidate)){
            return candidate;
        }

        // 若存在同名则在文件名后添加数字后缀，直到找到一个不存在的路径
        const auto stem = srcPath.stem().string();
        const auto ext = srcPath.extension().string();
        int suffix = 1;
        do {
            candidate = targetDir / (stem + "_" + std::to_string(suffix++) + ext);
        } while(std::filesystem::exists(candidate));
        return candidate;
    }

    bool EditorContentBrowser::CopyPathToDirectory(const std::filesystem::path& srcPath, const std::filesystem::path& targetDir)
    {
        if(srcPath.empty() || targetDir.empty() || !std::filesystem::exists(srcPath) || !std::filesystem::is_directory(targetDir)){
            return false;
        }

        std::error_code ec;
        auto srcParent = srcPath.parent_path();
        if(!srcParent.empty() && std::filesystem::equivalent(srcParent, targetDir, ec)){
            return false;
        }
        ec.clear();

        // 生成目标路径，若存在同名文件则添加后缀
        auto targetPath = MakeUniqueTargetPath(targetDir, srcPath);
        if(std::filesystem::is_directory(srcPath)){
            std::filesystem::copy(srcPath, targetPath, std::filesystem::copy_options::recursive, ec);
        }
        else{
            std::filesystem::copy_file(srcPath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
        }
        return !ec;
    }
}
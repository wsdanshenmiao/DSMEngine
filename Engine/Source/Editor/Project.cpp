#include "Project.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/ContentPaths.h"
#include "Runtime/Core/Macro.h"
#include "Editor/Serializer/Serializer.h"

#include <filesystem>
#include <imgui.h>

namespace DSM {
    namespace {
        std::filesystem::path AbsolutePath(const std::filesystem::path& path)
        {
            std::error_code error;
            const auto absolute = std::filesystem::absolute(path, error);
            return (error ? path : absolute).lexically_normal();
        }

        bool IsVirtualPath(const std::filesystem::path& path)
        {
            return !path.empty() && path.generic_string().starts_with("/");
        }

        std::string ToVirtualPath(const std::filesystem::path& path)
        {
            if (IsVirtualPath(path)) {
                return path.generic_string();
            }
            if (auto virtualPath = ContentPaths::ToVirtualPath(AbsolutePath(path)); virtualPath) {
                return virtualPath->ToString();
            }
            return {};
        }

        bool IsProjectVirtualPath(std::string_view value)
        {
            auto parsed = VirtualPath::Parse(value);
            return parsed.has_value() && parsed->IsProjectPath();
        }
    }

    std::filesystem::path Project::GetProjectRoot() const
    {
        return m_FilePath.empty() ? std::filesystem::path{} : AbsolutePath(m_FilePath).parent_path();
    }

    std::filesystem::path Project::GetContentDirectory() const
    {
        return GetProjectRoot() / s_ContentFolderName;
    }

    std::filesystem::path Project::GetIntermediateDirectory() const
    {
        return GetProjectRoot() / s_IntermediateFolderName;
    }

    std::filesystem::path Project::GetSavedDirectory() const
    {
        return GetProjectRoot() / s_SavedFolderName;
    }

    std::filesystem::path Project::ResolveProjectPath(const std::filesystem::path& path) const
    {
        if (path.empty()) {
            return {};
        }
        if (IsVirtualPath(path)) {
            auto resolved = ContentPaths::ResolveVirtualForRead(path.generic_string());
            return resolved ? *resolved : std::filesystem::path{};
        }
        if (path.is_absolute()) {
            return path.lexically_normal();
        }
        return (GetProjectRoot() / path).lexically_normal();
    }

    std::filesystem::path Project::ResolveAssetPath(const std::filesystem::path& path) const
    {
        if (path.empty()) {
            return {};
        }
        if (IsVirtualPath(path)) {
            auto resolved = ContentPaths::ResolveVirtualForRead(path.generic_string());
            return resolved ? *resolved : std::filesystem::path{};
        }
        if (path.is_absolute()) {
            return path.lexically_normal();
        }
        auto resolved = ContentPaths::ResolveVirtualForRead(
            "/Game/" + path.generic_string());
        return resolved ? *resolved : std::filesystem::path{};
    }

    std::string Project::MakeProjectRelativePath(const std::filesystem::path& path) const
    {
        return ToVirtualPath(path);
    }

    void Project::NewProject()
    {
        auto& project = Project::GetInstance();
        NewScene();
        if (!project.m_FilePath.empty()) {
            SaveProject(project.m_FilePath);
        }
    }

    bool Project::LoadProject(const std::string& filepath)
    {
        if (filepath.empty()) {
            return false;
        }

        const auto projectPath = AbsolutePath(filepath);
        if (projectPath.extension() != s_ProjectFileExtension) {
            DSM_CORE_WARN("Could not load file {}, is not a project file", filepath);
            return false;
        }

        Project loadedProject;
        if (!Serializer::DeserializeFromFile(projectPath.string(), loadedProject)) {
            return false;
        }
        loadedProject.m_FilePath = projectPath.string();
        if (!loadedProject.m_SceneFilePath.empty() &&
            !IsProjectVirtualPath(loadedProject.m_SceneFilePath)) {
            DSM_CORE_WARN("项目启动场景必须使用 /Game/ 虚拟路径: {}", loadedProject.m_SceneFilePath);
            return false;
        }

        const auto previousName = m_Name;
        const auto previousFilePath = m_FilePath;
        const auto previousSceneFilePath = m_SceneFilePath;
        const auto previousStartupTarget = m_StartupTarget;

        ContentPaths::SetProjectRoot(projectPath.parent_path());
        auto newScene = std::make_shared<Scene>();
        const auto sceneVirtualPath = loadedProject.m_SceneFilePath;
        const auto scenePath = ResolveProjectPath(sceneVirtualPath);
        if (!sceneVirtualPath.empty()) {
            if (scenePath.empty() || !Serializer::DeserializeFromFile(scenePath.string(), *newScene)) {
                DSM_CORE_WARN("Could not load scene file {}", sceneVirtualPath);
                m_Name = previousName;
                m_FilePath = previousFilePath;
                m_SceneFilePath = previousSceneFilePath;
                m_StartupTarget = previousStartupTarget;
                ContentPaths::SetProjectRoot(
                    previousFilePath.empty() ? std::filesystem::path{} : AbsolutePath(previousFilePath).parent_path());
                return false;
            }
            newScene->SetSceneFilePath(sceneVirtualPath);
        }

        if (!m_FilePath.empty()) {
            SaveProject(m_FilePath);
        }

        m_Name = loadedProject.m_Name;
        m_StartupTarget = loadedProject.m_StartupTarget;
        m_FilePath = projectPath.string();
        m_SceneFilePath = sceneVirtualPath;
        ContentPaths::SetProjectRoot(projectPath.parent_path());
        DSMEngine::sm_GlobalContext.scene = std::move(newScene);
        return true;
    }

    void Project::SaveProject(const std::string& filepath)
    {
        if (filepath.empty()) {
            return;
        }

        const auto projectPath = AbsolutePath(filepath);
        if (projectPath.extension() != s_ProjectFileExtension) {
            return;
        }

        auto& project = Project::GetInstance();
        project.m_FilePath = projectPath.string();
        ContentPaths::SetProjectRoot(projectPath.parent_path());

        auto& scene = DSMEngine::sm_GlobalContext.scene;
        if (scene != nullptr && scene->IsDirty()) {
            if (scene->GetSceneFilePath().empty()) {
                SaveScene("/Game/Scenes/Untitled.dsmscene");
            }
            else {
                SaveScene(scene->GetSceneFilePath());
            }
        }
        if (scene != nullptr) {
            project.m_SceneFilePath = scene->GetSceneFilePath();
        }

        std::filesystem::create_directories(projectPath.parent_path());
        Serializer::SerializeToFile(projectPath.string(), project);
    }

    void Project::NewScene()
    {
        auto& scene = DSMEngine::sm_GlobalContext.scene;
        if (scene != nullptr && scene->IsDirty() && !scene->GetSceneFilePath().empty()) {
            SaveScene(scene->GetSceneFilePath());
        }

        scene = std::make_shared<Scene>();
        scene->SetSceneFilePath({});
        m_SceneFilePath.clear();
    }

    void Project::LoadScene(const std::string& filepath)
    {
        if (filepath.empty() || m_FilePath.empty()) {
            return;
        }

        const auto physicalPath = AbsolutePath(filepath);
        const auto sceneVirtualPath = IsVirtualPath(filepath)
            ? filepath : ToVirtualPath(physicalPath);
        if (!IsProjectVirtualPath(sceneVirtualPath) ||
            std::filesystem::path(sceneVirtualPath).extension() != s_SceneFileExtension) {
            DSM_CORE_WARN("Could not load file {}, is not a scene file", filepath);
            return;
        }

        const auto scenePath = ResolveProjectPath(sceneVirtualPath);
        auto newScene = std::make_shared<Scene>();
        if (scenePath.empty() || !Serializer::DeserializeFromFile(scenePath.string(), *newScene)) {
            DSM_CORE_WARN("Could not load scene file {}", sceneVirtualPath);
            return;
        }

        auto& currentScene = DSMEngine::sm_GlobalContext.scene;
        if (currentScene != nullptr && currentScene->IsDirty() && !currentScene->GetSceneFilePath().empty()) {
            SaveScene(currentScene->GetSceneFilePath());
        }

        newScene->SetSceneFilePath(sceneVirtualPath);
        currentScene = std::move(newScene);
        m_SceneFilePath = sceneVirtualPath;
    }

    void Project::SaveScene(const std::string& filepath)
    {
        if (filepath.empty()) {
            return;
        }

        auto& scene = DSMEngine::sm_GlobalContext.scene;
        const auto virtualPath = IsVirtualPath(filepath) ? filepath : ToVirtualPath(filepath);
        if (scene == nullptr || !scene->IsDirty() || !IsProjectVirtualPath(virtualPath) ||
            std::filesystem::path(virtualPath).extension() != s_SceneFileExtension) {
            return;
        }

        auto physicalPath = ContentPaths::ResolveVirtualForWrite(virtualPath);
        if (!physicalPath) {
            DSM_CORE_WARN("场景保存路径不可写: {}", virtualPath);
            return;
        }
        std::filesystem::create_directories(physicalPath->parent_path());
        if (Serializer::SerializeToFile(physicalPath->string(), *scene)) {
            scene->SetSceneFilePath(virtualPath);
            scene->SetDirty(false);
            m_SceneFilePath = virtualPath;
        }
    }
}

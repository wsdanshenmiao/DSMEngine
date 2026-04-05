#include "Project.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Macro.h"
#include "Editor/Serializer/Serializer.h"

#include <filesystem>
#include <imgui.h>

namespace DSM {
    void Project::NewProject()
    {
        auto& project = Project::GetInstance();
        if(project.m_FilePath.empty()){
            SaveProject(project.m_FilePath);
        }
        NewScene();
    }

    bool Project::LoadProject(const std::string &filepath)
    {
        std::filesystem::path path = filepath;
        // 检测文件名的后缀
        if(path.extension() != s_ProjectFileExtension){
            DSM_CORE_WARN("Could not load file {}, is not a project file", filepath);
            return false;
        }

        if(filepath.empty()) {
            return false;
        }

        // 保存当前的项目
        if(!m_FilePath.empty()){
            SaveProject(m_FilePath);
        }

        Project loadedProject;
        if(!Serializer::DeserializeFromFile(filepath, loadedProject)){
            return false;
        }

        auto newScene = std::make_shared<Scene>();
        const auto& sceneFilePath = loadedProject.m_SceneFilePath;
        if(!sceneFilePath.empty()){
            if(!Serializer::DeserializeFromFile(sceneFilePath, *newScene)){
                DSM_CORE_WARN("Could not load scene file {}", sceneFilePath);
                return false;
            }

            newScene->SetSceneFilePath(sceneFilePath);
        }

        auto& currentProject = Project::GetInstance();
        currentProject.m_Name = loadedProject.m_Name;
        currentProject.m_FilePath = filepath;
        currentProject.m_SceneFilePath = newScene->GetSceneFilePath();

        DSMEngine::sm_GlobalContext.scene = newScene;
        return true;
    }
    
    void Project::SaveProject(const std::string &filepath)
    {
        std::filesystem::path path = filepath;
        if(filepath.empty() || path.extension() != Project::s_ProjectFileExtension) {
            return;
        }

        auto& project = Project::GetInstance();

        // 保存场景
        auto& scene = DSMEngine::sm_GlobalContext.scene;
        if(scene != nullptr){
            if(scene->IsDirty()){
                if(scene->GetSceneFilePath().empty()){
                    auto sceneFilePath = Utility::FileDialogs::SaveFile({{"DSM Scene File", "*" + std::string(s_SceneFileExtension)}}, "Save Scene");
                    SaveScene(sceneFilePath.empty() ? "" : sceneFilePath[0]);
                }
                else{
                    SaveScene(scene->GetSceneFilePath());
                }
            }
            project.m_SceneFilePath = scene->GetSceneFilePath();
        }

        project.m_FilePath = filepath;
        Serializer::SerializeToFile(filepath, project);
    }
    
    void Project::NewScene()
    {
        // 先保存当前的场景
        auto& scene = DSMEngine::sm_GlobalContext.scene;
        if(scene != nullptr){
            SaveScene(scene->GetSceneFilePath());
        }

        scene = std::make_shared<Scene>();
        scene->SetSceneFilePath({});
        m_SceneFilePath = {};
    }
    
    void Project::LoadScene(const std::string &filepath)
    {
        if(filepath.empty()) {
            return;
        }

        // 加载新场景前先保存当前的场景
        auto& scene = DSMEngine::sm_GlobalContext.scene;
        if(scene != nullptr){
            if(scene->GetSceneFilePath().empty()){
                auto sceneFilePath = Utility::FileDialogs::SaveFile({{"DSM Scene File", "*" + std::string(s_SceneFileExtension)}}, "Save Scene");
                SaveScene(sceneFilePath.empty() ? "" : sceneFilePath[0]);
            }
            else {
                SaveScene(scene->GetSceneFilePath());
            }
        }
        
        std::filesystem::path path = filepath;
        // 检测文件名的后缀
        if(path.extension() != s_SceneFileExtension){
            DSM_CORE_WARN("Could not load file {}, is not a scene file", filepath);
            return;
        }
        scene = std::make_shared<Scene>();
        if(Serializer::DeserializeFromFile(filepath, *DSMEngine::sm_GlobalContext.scene)){
            scene->SetSceneFilePath(filepath);
            m_SceneFilePath = filepath;
        }
    }
    
    void Project::SaveScene(const std::string &filepath)
    {
        std::filesystem::path path = filepath;
        auto& scene = DSMEngine::sm_GlobalContext.scene;
        if(scene == nullptr || !scene->IsDirty() || filepath.empty() ||
            path.extension() != Project::s_SceneFileExtension) {
            return;
        }
        if(Serializer::SerializeToFile(filepath, *scene)){
            scene->SetSceneFilePath(filepath);
            scene->SetDirty(false);
            m_SceneFilePath = filepath;
        }
    }
}
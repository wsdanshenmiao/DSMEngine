#include "SceneManager.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Macro.h"
#include "Editor/AssertDefine.h"
#include "Editor/Serializer/Serializer.h"

#include <filesystem>

namespace DSM {
    void SceneManager::NewScene()
    {
        DSMEngine::sm_GlobalContext.scene = std::make_shared<Scene>();
    }
    
    void SceneManager::LoadScene(const std::string &filepath)
    {
        std::filesystem::path path = filepath;
        // 检测文件名的后缀
        if(path.extension() != g_SceneFileExtension){
            DSM_CORE_WARN("Could not load file {}, is not a scene file", filepath);
            return;
        }

        if(!filepath.empty()) {
            auto newScene = std::make_shared<Scene>();
            if(Serializer::DeserializeFromFile(filepath, *newScene)){
                DSMEngine::sm_GlobalContext.scene = newScene;
            }
        }
    }
    
    void SceneManager::SaveScene(const std::string &filepath)
    {
        if (!filepath.empty()) {
            Serializer::SerializeToFile(filepath, *DSMEngine::sm_GlobalContext.scene);
        }
    }
}
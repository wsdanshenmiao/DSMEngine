#include "SceneManager.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/Macro.h"
#include "Editor/AssertDefine.h"
#include "Editor/SceneSerializer.h"

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

        if(!filepath.empty()){
            SceneSerializer serializer;
            if(auto scene = serializer.Deserialize(filepath)){
                DSMEngine::sm_GlobalContext.scene = scene;
            }
        }
    }
    
    void SceneManager::SaveScene(const std::string &filepath)
    {
        if (!filepath.empty()) {
            SceneSerializer serializer;
            serializer.Serialize(filepath, DSMEngine::sm_GlobalContext.scene);
        }
    }
}
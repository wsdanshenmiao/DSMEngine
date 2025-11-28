#include "SceneSerializer.h"
#include "Runtime/DSMEngine.h"

#include <fstream>

using namespace nlohmann;

namespace DSM {
    
    void SceneSerializer::Serialize(const std::string &filepath)
    {
        std::filesystem::path path = filepath;
        if (!std::filesystem::exists(path.parent_path())) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(filepath);
        if(file.is_open()){
            json sceneJson{};
            sceneJson["objects"] = json::array();
            for (const auto& [id, objectPtr] : DSMEngine::sm_GlobalContext.scene->GetAllObjects()) {
                auto& obj = *objectPtr;
                json objJson{};
                objJson["enabled"] = obj.IsEnabled();
                if(obj.HasComponent<DSM::TagComponent>()){
                    objJson["tag"] = *obj.GetComponent<DSM::TagComponent>();
                }
                if(obj.HasComponent<DSM::Math::Transform>()){
                    objJson["transform"] = *obj.GetComponent<DSM::Math::Transform>();
                }
                if(obj.HasComponent<Camera>()){
                    objJson["camera"] = *obj.GetComponent<Camera>();
                }
                if(obj.HasComponent<DSM::Model>()){
                    objJson["model"] = *obj.GetComponent<DSM::Model>();
                }
                sceneJson["objects"].push_back(objJson);
            }
            
            file << sceneJson.dump(4);
            file.close();
        }
    }
    
    bool SceneSerializer::Deserialize(const std::string &filepath)
    {
        std::ifstream file{filepath};
        if(file.is_open() && std::filesystem::file_size(filepath) > 0){
            json sceneJson;
            file >> sceneJson;
            
            if(!sceneJson.contains("objects"))
                return false;

            DSMEngine::sm_GlobalContext.scene = std::make_shared<Scene>();

            for (const auto& objJson : sceneJson.at("objects")) {
                auto objID = DSMEngine::sm_GlobalContext.scene->CreateObject();
                auto objPtr = DSMEngine::sm_GlobalContext.scene->GetObjectByID(objID).lock();
                objPtr->SetEnabled(objJson.at("enabled").get<bool>());
                if(objJson.contains("tag")){
                    objPtr->GetComponent<DSM::TagComponent>()->tag = objJson.at("tag").get<DSM::TagComponent>().tag;
                }
                if(objJson.contains("transform")){
                    *objPtr->GetComponent<DSM::Math::Transform>() = objJson.at("transform").get<DSM::Math::Transform>();
                }
                if(objJson.contains("camera")){
                    objPtr->AddComponent<Camera>();
                    *objPtr->GetComponent<Camera>() = objJson.at("camera").get<Camera>();
                }
                if(objJson.contains("model")){
                    objPtr->AddComponent<DSM::Model>();
                    *objPtr->GetComponent<DSM::Model>() = objJson.at("model").get<DSM::Model>();
                }
            }

            file.close();
            return true;
        }
        return false;
    }
}
#pragma once
#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__


#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Math/Transform.h"
#include "Runtime/Render/Camera/Camera.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Framework/Scene.h"


#include <nlohmann/json.hpp>
#include <fstream>


namespace DSM {
    class Serializer
    {
    public:
        template <typename T>
        static nlohmann::json Serialize(const T& obj) 
        {
            nlohmann::json j;
            j = obj;
            return j;
        }

        template <typename T>
        static void Deserialize(const nlohmann::json& j, T& obj) 
        {
            obj = j.get<T>();
        }

        template <typename T>
        static bool SerializeToFile(const std::string& filepath, const T& obj) 
        {
            std::filesystem::path path = filepath;
            if(!path.parent_path().empty() && !std::filesystem::exists(path.parent_path())) {
                std::filesystem::create_directories(path.parent_path());
            }

            std::ofstream file{path};
            bool success = file.is_open();
            if(success){
                nlohmann::json j = Serialize<T>(obj);
                file << j.dump(4);
                file.close();
            }
            return success;
        }

        template <typename T>
        static bool DeserializeFromFile(const std::string& filepath, T& obj) 
        {
            std::ifstream file{filepath};
            if(!file.is_open() || std::filesystem::file_size(filepath) <= 0){
                return false;
            }

            nlohmann::json j;
            file >> j;
            file.close();
            Deserialize<T>(j, obj);

            return true;
        }

    };
}


namespace nlohmann {
    template<>
    struct adl_serializer<DSM::Math::Scalar> {
        static void to_json(json& j, const DSM::Math::Scalar& scalar) {
            j = static_cast<float>(scalar);
        }
        static void from_json(const json& j, DSM::Math::Scalar& scalar) {
            scalar = DSM::Math::Scalar(j.get<float>());
        }
    };

    template<>
    struct adl_serializer<DSM::Math::Vector3> {
        static void to_json(json& j, const DSM::Math::Vector3& vec) {
            j = json::array({vec.Get(0), vec.Get(1), vec.Get(2)});
        }
        static void from_json(const json& j, DSM::Math::Vector3& vec) {
            vec = DSM::Math::Vector3{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
        }
    };

    template<>
    struct adl_serializer<DSM::Math::Vector4> {
        static void to_json(json& j, const DSM::Math::Vector4& vec) {
            j = json::array({vec.Get(0), vec.Get(1), vec.Get(2), vec.Get(3)});
        }
        static void from_json(const json& j, DSM::Math::Vector4& vec) {
            vec = DSM::Math::Vector4{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>()};
        }
    };

    template<>
    struct adl_serializer<DSM::Math::Quaternion> {
        static void to_json(json& j, const DSM::Math::Quaternion& quat) {
            j = json::array({quat.Get(0), quat.Get(1), quat.Get(2), quat.Get(3)});
        }

        static void from_json(const json& j, DSM::Math::Quaternion& quat) {
            quat = DSM::Math::Quaternion{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>()};
        }
    };

    template<>
    struct adl_serializer<DSM::TagComponent> {
        static void to_json(json& j, const DSM::TagComponent& tag) {
            j = json{{"tag", tag.tag}};
        }

        static void from_json(const json& j, DSM::TagComponent& tag) {
            if(j.contains("tag")){
                tag.tag = j.at("tag").get<std::string>();
            }
        }
    };

    template<>
    struct adl_serializer<DSM::Math::Transform> {
        static void to_json(json& j, const DSM::Math::Transform& trans) {
            j = {{"position", trans.GetPosition()},
                {"scale", trans.GetScale()},
                {"rotation", trans.GetRotation()}};
        }

        static void from_json(const json& j, DSM::Math::Transform& trans) {
            auto getData = [&j] <typename T> (const std::string& name, T& data){
                data = j.contains(name) ? j.at(name).get<T>() : T{};
            };
            DSM::Math::Vector3 vec3;
            getData("position", vec3);
            trans.SetPosition(vec3);
            getData("scale", vec3);
            trans.SetScale(vec3);
            DSM::Math::Quaternion quat;
            getData("rotation", quat);
            trans.SetRotation(quat);
        }
    };

    template<>
    struct adl_serializer<DSM::Camera> {
        static void to_json(json& j, const DSM::Camera& camera) {
            j = { {"fovY", camera.GetFovY()},
                {"nearZ", camera.GetNearZ()},
                {"farZ", camera.GetFarZ()},
                {"reversedZ", camera.IsReversedZ()} };
        }
        static void from_json(const json& j, DSM::Camera& camera) {
            auto getData = [&j] <typename T> (const std::string& name, T& data){
                data = j.contains(name) ? j.at(name).get<T>() : T{};
            };
            float fovY;
            getData("fovY", fovY);
            camera.SetFovY(fovY);
            float nearZ;
            getData("nearZ", nearZ);
            camera.SetNearZ(nearZ);
            float farZ;
            getData("farZ", farZ);
            camera.SetFarZ(farZ);
            bool reversedZ;
            getData("reversedZ", reversedZ);
            camera.ReverseZ(reversedZ);
        }
    };

    template<>
    struct adl_serializer<DSM::Model> {
        static void to_json(json& j, const DSM::Model& model) {
            j = { {"filePath", model.filePath} };
        }
        static void from_json(const json& j, DSM::Model& model) {
            if(j.contains("filePath")){
                model.filePath = j.at("filePath").get<std::string>();
            }
            auto newModel = DSM::ModelLoader::LoadModel(model.filePath);
            if(newModel != nullptr){
                model = std::move(*newModel);
            }
        }
    };

    template<>
    struct adl_serializer<DSM::Scene> {
        static void to_json(json& sceneJson, const DSM::Scene& scene) {
            sceneJson["objects"] = json::array();
            for (const auto& [id, objectPtr] : scene.GetAllObjects()) {
                auto& obj = *objectPtr;
                json objJson{};
                objJson["enabled"] = obj.IsEnabled();
                if(obj.HasComponent<DSM::TagComponent>()){
                    objJson["tag"] = *obj.GetComponent<DSM::TagComponent>();
                }
                if(obj.HasComponent<DSM::Math::Transform>()){
                    objJson["transform"] = *obj.GetComponent<DSM::Math::Transform>();
                }
                if(obj.HasComponent<DSM::Camera>()){
                    objJson["camera"] = *obj.GetComponent<DSM::Camera>();
                }
                if(obj.HasComponent<DSM::Model>()){
                    objJson["model"] = *obj.GetComponent<DSM::Model>();
                }
                sceneJson["objects"].push_back(objJson);
            }
        }
        static void from_json(const json& sceneJson, DSM::Scene& scene) {
            for (const auto& objJson : sceneJson.at("objects")) {
                auto objID = scene.CreateObject();
                auto objPtr = scene.GetObjectByID(objID).lock();
                objPtr->SetEnabled(objJson.at("enabled").get<bool>());
                if(objJson.contains("tag")){
                    objPtr->GetComponent<DSM::TagComponent>()->tag = objJson.at("tag").get<DSM::TagComponent>().tag;
                }
                if(objJson.contains("transform")){
                    *objPtr->GetComponent<DSM::Math::Transform>() = objJson.at("transform").get<DSM::Math::Transform>();
                }
                if(objJson.contains("camera")){
                    objPtr->AddComponent<DSM::Camera>();
                    *objPtr->GetComponent<DSM::Camera>() = objJson.at("camera").get<DSM::Camera>();
                }
                if(objJson.contains("model")){
                    objPtr->AddComponent<DSM::Model>();
                    *objPtr->GetComponent<DSM::Model>() = objJson.at("model").get<DSM::Model>();
                }
            }
        }
    };
}

#endif
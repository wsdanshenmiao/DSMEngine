#pragma once
#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__


#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Framework/Component/CameraComponent.h"
#include "Runtime/Framework/Component/Light.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Framework/Component/NativeScript.h"
#include "Runtime/Render/ModelLoader.h"
#include "Runtime/Framework/Scene.h"
#include "Editor/Project.h"


#include <nlohmann/json.hpp>
#include <fstream>
#include <queue>


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
            j.get_to(obj);
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
    struct adl_serializer<DSM::TransformComponent> {
        static void to_json(json& j, const DSM::TransformComponent& trans) {
            j = {{"position", trans.GetPosition()},
                {"scale", trans.GetScale()},
                {"rotation", trans.GetRotation()}};
        }

        static void from_json(const json& j, DSM::TransformComponent& trans) {
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
    struct adl_serializer<DSM::CameraComponent> {
        static void to_json(json& j, const DSM::CameraComponent& camera) {
            j = { {"fovY", camera.GetFovY()},
                {"nearZ", camera.GetNearZ()},
                {"farZ", camera.GetFarZ()},
                {"reversedZ", camera.IsReversedZ()} };
        }
        static void from_json(const json& j, DSM::CameraComponent& camera) {
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
    struct adl_serializer<DSM::Light> {
        static void to_json(json& j, const DSM::Light& light) {
            j = { {"type", light.GetType()},
                {"color", light.GetColor()},
                {"direction", light.GetDirection()},
                {"position", light.GetPosition()},
                {"range", light.GetRange()},
                {"innerAngle", light.GetInnerAngle()},
                {"outerAngle", light.GetOuterAngle()} };
        }
        static void from_json(const json& j, DSM::Light& light) {
            auto getData = [&j] <typename T> (const std::string& name, T& data){
                data = j.contains(name) ? j.at(name).get<T>() : T{};
            };
            DSM::LightType type;
            getData("type", type);
            light.SetType(type);
            DSM::Math::Vector4 color;
            getData("color", color);
            light.SetColor(color);
            DSM::Math::Vector3 direction;
            getData("direction", direction);
            light.SetDirection(direction);
            DSM::Math::Vector3 position;
            getData("position", position);
            light.SetPosition(position);
            float range;
            getData("range", range);
            light.SetRange(range);
            float innerAngle;
            getData("innerAngle", innerAngle);
            light.SetInnerAngle(innerAngle);
            float outerAngle;
            getData("outerAngle", outerAngle);
            light.SetOuterAngle(outerAngle);
        }
    };

    template<>
    struct adl_serializer<DSM::MeshRenderer> {
        static void to_json(json& j, const DSM::MeshRenderer& renderer) {
        }
        static void from_json(const json& j, DSM::MeshRenderer& renderer) {
        }
    };

    template<>
    struct adl_serializer<DSM::NativeScript> {
        static void to_json(json& j, const DSM::NativeScript& script) {
            j = { {"enabled", script.IsEnabled()} };
        }
        static void from_json(const json& j, DSM::NativeScript& script) {
            if(j.contains("enabled")){
                script.SetEnabled(j.at("enabled").get<bool>());
            }
        }
    };

    template<>
    struct adl_serializer<DSM::Scene> {
        static void to_json(json& sceneJson, const DSM::Scene& scene) {
            auto travalAllComponents = [&]<typename... Components>(DSM::type_list<Components...>, auto obj, json& objJson){
                auto saveComponentForObject = [&objJson] <typename T> (auto objectPtr){
                    if(objectPtr != nullptr && objectPtr->HasComponent<T>()){
                        objJson[typeid(T).name()] = *objectPtr->GetComponent<T>();
                    }
                };
                (saveComponentForObject.template operator()<Components>(obj), ...);
            };

            auto& objects = sceneJson["objects"] = json::array();
            // 需要先遍历父物体，以保存物体的层级关系，否则在反序列化时无法正确设置父子关系
            for(const auto& objPtr : scene.GetRootObjects()){
                // 广度优先遍历物体层级
                std::queue<std::pair<std::shared_ptr<DSM::GameObject>, ptrdiff_t>> objQueue{};
                objQueue.push({objPtr, -1});
                while(!objQueue.empty()){
                    auto [obj, parentIndex] = objQueue.front();
                    objQueue.pop();

                    for(const auto& child : obj->GetChildren()){
                        objQueue.emplace(child, objects.size());
                    }

                    json objJson{};
                    objJson["enabled"] = obj->IsEnabled();
                    objJson["tag"] = obj->GetTag();
                    objJson["name"] = obj->GetName();
                    objJson["parent"] = parentIndex;
                    travalAllComponents(DSM::AllComponents{}, obj, objJson); 
                    objects.push_back(objJson);
                }
            }
            
            sceneJson["sceneFilePath"] = scene.GetSceneFilePath();
            sceneJson["isDirty"] = false;
        }

        static void from_json(const json& sceneJson, DSM::Scene& scene) {
            auto travalAllComponents = [] <typename... Component> 
                (DSM::type_list<Component...>, const auto& objJson, auto obj){
                auto getData = [&objJson, &obj] <typename T> (){
                    if(objJson.contains(typeid(T).name())){
                        auto component = obj->AddOrReplaceComponent<T>();
                        objJson.at(typeid(T).name()).get_to(*component);
                    }
                };
                (getData.operator()<Component>(), ...);
            };
            scene.SetSceneFilePath(sceneJson.value("sceneFilePath", std::string{}));
            scene.SetDirty(sceneJson.value("isDirty", false));
            if(!sceneJson.contains("objects") || !sceneJson.at("objects").is_array()){
                return;
            }

            std::vector<DSM::ObjectID> parentIndices{};
            auto& objects = sceneJson.at("objects");
            for (const auto& objJson : objects) {
                auto objID = scene.CreateObject();
                auto objPtr = scene.GetObjectByID(objID).lock();
                ptrdiff_t parentIndex = objJson.value("parent", -1);
                if(0 <= parentIndex && parentIndex < parentIndices.size()){
                    objPtr->SetParent(scene.GetObjectByID(parentIndices[parentIndex]).lock());
                }
                objPtr->SetEnabled(objJson.at("enabled").get<bool>());
                if(objJson.contains("tag")){
                    objPtr->SetTag(objJson.at("tag").get<std::string>());
                }
                if(objJson.contains("name")){
                    objPtr->SetName(objJson.at("name").get<std::string>());
                }
				travalAllComponents(DSM::AllComponents{}, objJson, objPtr);
                parentIndices.push_back(objID);
            }
        }
    };

    template<>
    struct adl_serializer<DSM::Project> {
        static void to_json(json& j, const DSM::Project& project) {
            j = {{"name", project.GetProjectName()},
                {"filePath", project.GetFilePath()},
                {"sceneFilePath", project.GetSceneFilePath()}};
        }

        static void from_json(const json& j, DSM::Project& project) {
            project.SetProjectName(j.value("name", std::string{}));
            project.SetFilePath(j.value("filePath", std::string{}));
            project.SetSceneFilePath(j.value("sceneFilePath", std::string{}));
        }
    };
}

#endif
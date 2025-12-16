#ifndef __SCENE_SERIALIZER_H__
#define __SCENE_SERIALIZER_H__

#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Math/Transform.h"
#include "Runtime/Render/Camera/Camera.h"
#include "Runtime/Render/ModelLoader.h"

#include <nlohmann/json.hpp>


namespace DSM {
    class SceneSerializer
    {
    public:
        void Serialize(const std::string& filepath, std::shared_ptr<Scene> scene);
        std::shared_ptr<Scene> Deserialize(const std::string& filepath);
    };
} // namespace DSM



namespace nlohmann {
    template<>
    struct adl_serializer<DSM::Math::Scalar>
    {
        static void to_json(json& j, const DSM::Math::Scalar& scalar)
        {
            j = static_cast<float>(scalar);
        }
        static void from_json(const json& j, DSM::Math::Scalar& scalar)
        {
            scalar = DSM::Math::Scalar(j.get<float>());
        }
    };

    template<>
    struct adl_serializer<DSM::Math::Vector3>
    {
        static void to_json(json& j, const DSM::Math::Vector3& vec)
        {
            j = json::array({vec.Get(0), vec.Get(1), vec.Get(2)});
        }
        static void from_json(const json& j, DSM::Math::Vector3& vec)
        {
            vec = DSM::Math::Vector3{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
        }
    };

    template<>
    struct adl_serializer<DSM::Math::Vector4>
    {
        static void to_json(json& j, const DSM::Math::Vector4& vec)
        {
            j = json::array({vec.Get(0), vec.Get(1), vec.Get(2), vec.Get(3)});
        }
        static void from_json(const json& j, DSM::Math::Vector4& vec)
        {
            vec = DSM::Math::Vector4{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>()};
        }
    };

    template<>
    struct adl_serializer<DSM::Math::Quaternion>
    {
        static void to_json(json& j, const DSM::Math::Quaternion& quat)
        {
            j = json::array({quat.Get(0), quat.Get(1), quat.Get(2), quat.Get(3)});
        }

        static void from_json(const json& j, DSM::Math::Quaternion& quat)
        {
            quat = DSM::Math::Quaternion{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>()};
        }
    };

    template<>
    struct adl_serializer<DSM::TagComponent>
    {
        static void to_json(json& j, const DSM::TagComponent& tag)
        {
            j = json{{"tag", tag.tag}};
        }

        static void from_json(const json& j, DSM::TagComponent& tag)
        {
            if(j.contains("tag")){
                tag.tag = j.at("tag").get<std::string>();
            }
        }
    };

    template<>
    struct adl_serializer<DSM::Math::Transform>
    {
        static void to_json(json& j, const DSM::Math::Transform& trans)
        {
            j = {{"position", trans.GetPosition()},
                {"scale", trans.GetScale()},
                {"rotation", trans.GetRotation()}};
        }

        static void from_json(const json& j, DSM::Math::Transform& trans)
        {
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
    struct adl_serializer<DSM::Camera>
    {
        static void to_json(json& j, const DSM::Camera& camera)
        {
            j = {
                {"fovY", camera.GetFovY()},
                {"nearZ", camera.GetNearZ()},
                {"farZ", camera.GetFarZ()},
                {"reversedZ", camera.IsReversedZ()}
            };
        }
        static void from_json(const json& j, DSM::Camera& camera)
        {
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
    struct adl_serializer<DSM::Model>
    {
        static void to_json(json& j, const DSM::Model& model)
        {
            j = { {"filePath", model.filePath} };
        }
        static void from_json(const json& j, DSM::Model& model)
        {
            if(j.contains("filePath")){
                model.filePath = j.at("filePath").get<std::string>();
            }
            auto newModel = DSM::ModelLoader::LoadModel(model.filePath);
            if(newModel != nullptr){
                model = std::move(*newModel);
            }
        }
    };
}

#endif
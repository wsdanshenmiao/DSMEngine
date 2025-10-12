#include "Scene.h"
#include "Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Framework/ScriptableObject.h"

namespace DSM {
    void Scene::Update(float deltaTime)
    {
        m_Registry.view<NativeScriptComponent>().each([this](entt::entity entity, NativeScriptComponent& script) {
            if(script.instance == nullptr){
                script.instance = script.InstantiateScript();
                script.instance->m_GameObject = GetObjectByID(entity).lock();
                script.instance->Awake();
                script.instance->Start();
            }

            script.instance->OnUpdate();
        });
    }

    void Scene::OnGUI()
    {
        m_Registry.view<NativeScriptComponent>().each([this](entt::entity entity, NativeScriptComponent& script) {
            if(script.instance == nullptr){
                script.instance = script.InstantiateScript();
                script.instance->m_GameObject = GetObjectByID(entity).lock();
                script.instance->Awake();
                script.instance->Start();
            }

            script.instance->OnGUI();
        });
    }

    std::weak_ptr<GameObject> Scene::GetObjectByID(ObjectID objectID) const
    {
        if(auto it = m_Objects.find(objectID); it != m_Objects.end())
            return it->second;
        else
            return std::weak_ptr<GameObject>();
    }

    ObjectID Scene::CreateObject(const std::string &name)
    {
        ObjectID id = m_Registry.create();
        auto object = std::make_shared<GameObject>(id, this);
        assert(object != nullptr);
        // 每个物体默认添加 Transform 组件
        object->AddComponent<Math::Transform>();
        object->AddComponent<TagComponent>(name.empty() ? "GameObject" : name);
        m_Objects[id] = object;
        return id;
    }

    void Scene::DestroyObject(ObjectID objectID)
    {
        if(m_Objects.contains(objectID)){
            m_Objects.erase(objectID);
            m_Registry.destroy(objectID);
        }
    }
}
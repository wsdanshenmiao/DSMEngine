#include "Scene.h"
#include "Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Framework/ScriptableObject.h"

namespace DSM {
    Scene::~Scene()
    {
        m_Registry.clear();
        m_Objects.clear();
    }

    Scene::Scene(const Scene &src)
        :m_Registry(), m_Objects()
    {
        CopyScene(*this, src);
    }

    Scene &Scene::operator=(const Scene &src)
    {
        CopyScene(*this, src);
        return *this;
    }

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
    
    void Scene::CopyScene(Scene &dest, const Scene &src)
    {
        // 新场景与旧场景的对象ID映射表
        std::unordered_map<ObjectID, ObjectID> idMap;
        src.TraverseAllEntity([&](entt::entity entity) {
            if(auto it = src.m_Objects.find(entity); it != src.m_Objects.end()){
                auto oldGameObject = it->second;
                auto obj = dest.GetObjectByID(dest.CreateObject()).lock();
                obj->SetEnabled(oldGameObject->IsEnabled());
                idMap[entity] = obj->GetID();
            }
        });

        auto copyComponent = [&]<typename... Components>(std::variant<Components...>) {
            (src.GetAllObjectsWithComponents<Components>().each(
                [&](entt::entity entity, const Components& component) {
                    if(auto it = idMap.find(entity); it != idMap.end()){
                        if(auto obj = dest.GetObjectByID(it->second).lock(); obj != nullptr){
                            obj->AddOrReplaceComponent<Components>(component);
                        }
                    }
                }
            ), ...);
        };
        copyComponent(AllComponents{}); 
    }
}
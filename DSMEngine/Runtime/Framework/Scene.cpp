#include "Scene.h"
#include "Object/GameObject.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Framework/ScriptableObject.h"

#include <stack>

namespace DSM {

    Scene::~Scene()
    {
        m_Registry.clear();
        m_Objects.clear();
        m_RootObjects.clear();
    }

    Scene::Scene(const Scene &src)
        :m_Registry(), m_Objects(), m_RootObjects()
    {
        CopyScene(*this, src);
    }

    Scene &Scene::operator=(const Scene &src)
    {
        CopyScene(*this, src);
        return *this;
    }

    Scene::Scene(Scene &&other)
    {
        CopyScene(*this, other);
    }

    Scene &Scene::operator=(Scene &&other)
    {
        CopyScene(*this, other);
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
        m_RootObjects.insert(object);
        return id;
    }

    void Scene::DestroyObject(ObjectID objectID)
    {
        if(!m_Objects.contains(objectID))
            return;

        auto obj = m_Objects[objectID];
        if(obj->GetParent() == nullptr){
            m_RootObjects.erase(obj);
        }
        // 递归销毁子对象
        std::vector<std::shared_ptr<GameObject>> nodes{};
        std::stack<std::shared_ptr<GameObject>> toDestroy{};
        toDestroy.push(obj);
        while(!std::empty(toDestroy)){
            auto current = toDestroy.top();
            toDestroy.pop();
            nodes.push_back(current);
            for(auto& child : current->GetChildren()){
                toDestroy.push(child);
            }
        }

        for(auto& node : nodes | std::views::reverse){
            node->SetParent(nullptr);
            m_Objects.erase(node->GetID());
            m_Registry.destroy(node->GetID());
        }
    }
    
    void Scene::CopyScene(Scene &dest, const Scene &src)
    {
        // 新场景与旧场景的对象ID映射表
        std::unordered_map<ObjectID, ObjectID> idMap;
        src.TraverseAllEntity([&src, &dest, &idMap](entt::entity entity) {
            if(auto it = src.m_Objects.find(entity); it != src.m_Objects.end()){
                auto oldGameObject = it->second;
                auto obj = dest.GetObjectByID(dest.CreateObject()).lock();
                obj->SetEnabled(oldGameObject->IsEnabled());
                idMap[entity] = obj->GetID();
            }
        });
        // 根据映射关系复制父子关系
        dest.TraverseAllEntity([&src, &dest, &idMap](entt::entity entity) {
            if(auto destIt = dest.m_Objects.find(entity); destIt != dest.m_Objects.end()){
                auto destObj = destIt->second;
                if(auto srcIt = src.m_Objects.find(destIt->first); srcIt != src.m_Objects.end()){
                    auto parent = srcIt->second->GetParent();
                    auto destParentID = parent == nullptr ? entt::null : parent->GetID();
                    auto destParent = dest.GetObjectByID(destParentID).lock();
                    destObj->SetParent(destParent);
                }
            }
        });

        auto copyComponent = [&]<typename... Components>(std::variant<Components...>) {
            (src.GetObjectsWithComponents<Components>().each(
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
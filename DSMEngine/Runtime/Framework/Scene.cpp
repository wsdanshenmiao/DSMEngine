#include "Scene.h"
#include "Object/GameObject.h"
#include "Runtime/Framework/Component/Light.h"
#include "Runtime/Framework/Component/NativeScript.h"
#include "Runtime/Framework/ScriptableObject.h"
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Framework/Component/CameraComponent.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Core/Macro.h"

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
        m_Registry.view<NativeScript>().each([this](entt::entity entity, NativeScript& script) {
            auto scriptInstance = script.GetScript();
            if(scriptInstance == nullptr){
                return;
            }
            scriptInstance->OnUpdate();
        });
    }

    void Scene::OnGUI()
    {
        m_Registry.view<NativeScript>().each([this](entt::entity entity, NativeScript& script) {
            auto scriptInstance = script.GetScript();
            if(scriptInstance == nullptr){
                return;
            }
            scriptInstance->OnGUI();
        });
    }

    const std::weak_ptr<GameObject> Scene::GetObjectByID(ObjectID objectID) const
    {
        if(auto it = m_Objects.find(objectID); it != m_Objects.end()){
            return it->second;
        }
        else{
            return {};
        }
    }

    std::weak_ptr<GameObject> Scene::GetObjectByID(ObjectID objectID)
    {
        if(auto it = m_Objects.find(objectID); it != m_Objects.end()){
            m_IsDirty = true;
            return it->second;
        }
        else{
            return {};
        }
    }

    ObjectID Scene::CreateObject(const std::string &name)
    {
        ObjectID id = m_Registry.create();
        auto object = std::make_shared<GameObject>(id, this);
        DSM_CORE_ASSERT(object != nullptr);
        object->SetName(name.empty() ? "GameObject" : name);
        object->AddComponent<TransformComponent>();
        m_Objects[id] = object;
        
        m_RootObjects.insert(object);
        
        m_IsDirty = true;
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

        m_IsDirty = true;
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
                obj->SetName(oldGameObject->GetName());
                obj->SetTag(oldGameObject->GetTag());
                idMap[entity] = obj->GetID();
            }
        });

        auto getDestBySrcID = [&dest, &idMap](ObjectID srcID) -> std::shared_ptr<GameObject> {
            if (auto it = idMap.find(srcID); it != idMap.end()) {
                return dest.GetObjectByID(it->second).lock();
            }
            return nullptr;
        };

        src.GetObjectsWithComponents<TransformComponent>().each([&](entt::entity entity, const TransformComponent& transform) {
            auto destObj = getDestBySrcID(entity);
            if (destObj == nullptr) return;
            if (auto dstTransform = destObj->GetComponent<TransformComponent>(); dstTransform != nullptr) {
                *dstTransform = transform;
            }
        });

        src.GetObjectsWithComponents<CameraComponent>().each([&](entt::entity entity, const CameraComponent& camera) {
            auto destObj = getDestBySrcID(entity);
            if (destObj == nullptr) return;

            auto dstCamera = destObj->AddComponent<CameraComponent>();
            if (dstCamera == nullptr) {
                dstCamera = destObj->GetComponent<CameraComponent>();
            }
            if (dstCamera == nullptr) return;

            dstCamera->SetViewPort(camera.GetViewPort());
            dstCamera->SetFrustum(camera.GetFovY(), camera.GetAspectRatio(), camera.GetNearZ(), camera.GetFarZ());
            dstCamera->ReverseZ(camera.IsReversedZ());
        });

        src.GetObjectsWithComponents<Light>().each([&](entt::entity entity, const Light& light) {
            auto destObj = getDestBySrcID(entity);
            if (destObj == nullptr) return;

            auto dstLight = destObj->AddComponent<Light>();
            if (dstLight == nullptr) {
                dstLight = destObj->GetComponent<Light>();
            }
            if (dstLight == nullptr) return;

            dstLight->SetType(light.GetType())
                .SetColor(light.GetColor())
                .SetDirection(light.GetDirection())
                .SetPosition(light.GetPosition())
                .SetRange(light.GetRange())
                .SetInnerAngle(light.GetInnerAngle())
                .SetOuterAngle(light.GetOuterAngle());
        });

        src.GetObjectsWithComponents<MeshRenderer>().each([&](entt::entity entity, const MeshRenderer& meshRenderer) {
            auto destObj = getDestBySrcID(entity);
            if (destObj == nullptr) return;

            auto dstRenderer = destObj->AddComponent<MeshRenderer>();
            if (dstRenderer == nullptr) {
                dstRenderer = destObj->GetComponent<MeshRenderer>();
            }
            if (dstRenderer == nullptr) return;

            dstRenderer->SetRenderLayer(meshRenderer.GetRenderLayer());
            dstRenderer->SetCastShadow(meshRenderer.CastShadow());
            dstRenderer->SetReceiveShadow(meshRenderer.ReceiveShadow());
            dstRenderer->SetEnabled(meshRenderer.IsEnabled());
            dstRenderer->SetMaterial(meshRenderer.GetMaterial());
            if (auto mesh = meshRenderer.GetMesh(); mesh != nullptr) {
                dstRenderer->SetMesh(mesh);
            }
            dstRenderer->SetLocalBounds(meshRenderer.GetLocalBounds());
            dstRenderer->SetBounds(meshRenderer.GetBounds());
        });

        src.GetObjectsWithComponents<NativeScript>().each([&](entt::entity entity, const NativeScript& script) {
            auto destObj = getDestBySrcID(entity);
            if (destObj == nullptr) return;

            auto dstScript = destObj->AddComponent<NativeScript>();
            if (dstScript == nullptr) {
                dstScript = destObj->GetComponent<NativeScript>();
            }
            if (dstScript == nullptr) return;
            dstScript->SetEnabled(script.IsEnabled());
        });

        // 根据映射关系复制父子关系
        for (const auto& [srcID, dstID] : idMap) {
            auto srcObj = src.GetObjectByID(srcID).lock();
            auto dstObj = dest.GetObjectByID(dstID).lock();
            if (srcObj == nullptr || dstObj == nullptr) continue;

            auto srcParent = srcObj->GetParent();
            if (srcParent == nullptr) {
                dstObj->SetParent(nullptr);
                continue;
            }

            auto parentIt = idMap.find(srcParent->GetID());
            if (parentIt == idMap.end()) {
                dstObj->SetParent(nullptr);
                continue;
            }
            auto dstParent = dest.GetObjectByID(parentIt->second).lock();
            dstObj->SetParent(dstParent);
        }
    }
}
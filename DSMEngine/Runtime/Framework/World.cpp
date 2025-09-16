#include "World.h"
#include "Object/GameObject.h"


namespace DSM {
    void World::Update(float deltaTime)
    {

    }
    
    std::weak_ptr<GameObject> World::GetObject(GUID objectID) const
    {
        if(auto it = m_Objects.find(objectID); it != m_Objects.end())
            return it->second;
        else
            return std::weak_ptr<GameObject>();
    }

    GUID World::CreateObject(const std::string &name)
    {
        auto id = ObjectIDAllocator::AllocateID();
        auto object = std::make_shared<GameObject>(m_Registry.create(), id, this);
        assert(object != nullptr);
        object->SetName(name);
        m_Objects[object->GetID()] = object;
        return object->GetID();
    }

    void World::DestroyObject(GUID objectID)
    {
        if(m_Objects.contains(objectID)){
            m_Objects.erase(objectID);
            m_Registry.destroy(*m_Objects[objectID]);
        }
    }
}
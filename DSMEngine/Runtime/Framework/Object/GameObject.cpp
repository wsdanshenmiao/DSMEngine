#include "GameObject.h"

namespace DSM {
    GameObject::GameObject(ObjectID handle, Scene *world)
        : m_Handle(handle), m_World(world)
    {
    }
    
    void GameObject::AddChild(const std::shared_ptr<GameObject> &child)
    {
        // 避免添加自己为子对象
        if(child != nullptr && child.get() != this){
            child->SetParent(shared_from_this());
        }
    }

    void GameObject::AddChild(ObjectID childID)
    {
        if(auto child = m_World->GetObjectByID(childID).lock()){
            AddChild(child);
        }
    }

    void GameObject::RemoveChild(const std::shared_ptr<GameObject> &child)
    {
        if(child != nullptr && child->m_Parent.lock().get() == this){
            auto it = std::ranges::find(m_Children, child);
            if(it != m_Children.end()){
                (*it)->m_Parent.reset();
                m_Children.erase(it);
            }
        }
    }

    void GameObject::RemoveChild(ObjectID childID)
    {
        if(auto child = m_World->GetObjectByID(childID).lock()){
            RemoveChild(child);
        }
    }

    void GameObject::SetParent(const std::shared_ptr<GameObject> &parent)
    {
        // 避免设置自己为父对象
        if(parent == nullptr || parent.get() == this)
            return;

        // 检测若设置为父对象是否会成环
        for(auto curr = parent; curr != nullptr; curr = curr->m_Parent.lock()){
            if(curr.get() == this)
                return;
        }

        auto self = shared_from_this();
        if(parent != m_Parent.lock()){
            // 从当前父对象的子对象列表中移除
            if(auto currParent = m_Parent.lock(); currParent != nullptr){
                currParent->RemoveChild(self);
            }

            m_Parent = parent;
            // 添加到新父对象的子对象列表
            if(parent != nullptr){
                parent->m_Children.insert(self);
                // 如果之前没有父对象，则从场景的根对象列表中移除
                if(m_World->m_RootObjects.contains(self)){
                    m_World->m_RootObjects.erase(self);
                }
            }
            else{
                m_World->m_RootObjects.insert(self);
            }
        }
    }
    
    void GameObject::SetParent(ObjectID parentID)
    {
        if(auto parent = m_World->GetObjectByID(parentID).lock()){
            SetParent(parent);
        }
    }
}
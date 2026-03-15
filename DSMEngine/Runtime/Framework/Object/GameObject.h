#pragma once
#ifndef __GAMEOBJECT_H__
#define __GAMEOBJECT_H__

#include <memory>
#include <unordered_set>
#include "Runtime/Framework/Scene.h"


namespace DSM {
    class GameObject : public std::enable_shared_from_this<GameObject>
    {
    public:
        GameObject(ObjectID handle, Scene* scene);
        virtual ~GameObject() = default;

        virtual void Update(float deltaTime) {}

        ObjectID GetID() const noexcept { return m_Handle; }

        bool IsEnabled() const noexcept { return m_Enabled; }
        void SetEnabled(bool enabled) noexcept { m_Enabled = enabled; }

        template <typename T>
        bool HasComponent() const noexcept { return m_World->m_Registry.all_of<T>(m_Handle); }

        template <typename T>
        T* GetComponent() noexcept { return m_World->m_Registry.try_get<T>(m_Handle); }
        template <typename T>
        const T* GetComponent() const noexcept { return m_World->m_Registry.try_get<T>(m_Handle); }

        template <typename T, typename... Args>
        T* AddComponent(Args&&... args) noexcept 
        { 
            if(HasComponent<T>()) 
                return nullptr;
            else
                return &m_World->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
        }
        template <typename T, typename... Args>
        T* AddOrReplaceComponent(Args&&... args) noexcept 
        { 
            return &m_World->m_Registry.emplace_or_replace<T>(m_Handle, std::forward<Args>(args)...);
        }
        template <typename T>
        bool RemoveComponent() noexcept { if(HasComponent<T>()) { m_World->m_Registry.remove<T>(m_Handle); return true; } return false; }

        void AddChild(const std::shared_ptr<GameObject>& child);
        void RemoveChild(const std::shared_ptr<GameObject>& child);
        void SetParent(const std::shared_ptr<GameObject>& parent);
        const auto& GetChildren() const noexcept { return m_Children; }
        std::shared_ptr<GameObject> GetParent() const noexcept { return m_Parent.lock(); }

        bool operator==(const GameObject& other) const noexcept { return m_Handle == other.m_Handle && m_World == other.m_World; }

        operator ObjectID() const noexcept { return m_Handle; }

    protected:
        std::unordered_set<std::shared_ptr<GameObject>> m_Children{};
        std::weak_ptr<GameObject> m_Parent{};
        Scene* m_World;
        ObjectID m_Handle;
        bool m_Enabled = true;
    };
} // namespace DSM



#endif
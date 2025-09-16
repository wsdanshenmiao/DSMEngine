#pragma once
#ifndef __GAMEOBJECT_H__
#define __GAMEOBJECT_H__

#include <memory>
#include <vector>
#include "Runtime/Framework/World.h"


namespace DSM {
    class GameObject 
    {
    public:
        GameObject(entt::entity handle, GUID id, World* world);
        virtual ~GameObject() = default;

        virtual void Update(float deltaTime) {}

        GUID GetID() const noexcept { return m_ID; }
        const std::string& GetName() const noexcept { return m_Name; }
        void SetName(const std::string& name) noexcept { m_Name = name; }

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

        bool operator==(const GameObject& other) const noexcept { return m_ID == other.m_ID && m_World == other.m_World; }

        operator entt::entity() const noexcept { return m_Handle; }

    protected:
        GUID m_ID;
        std::string m_Name;
        entt::entity m_Handle;
        World* m_World;
    };
} // namespace DSM



#endif
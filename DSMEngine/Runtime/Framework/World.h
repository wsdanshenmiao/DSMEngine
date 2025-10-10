#pragma once
#ifndef __WORLD_H__
#define __WORLD_H__

#include <unordered_map>
#include <entt/entt.hpp>
#include "Object/ObjectIDAllocator.h"

namespace DSM {
    class GameObject;

    class World
    {
        friend class GameObject;
    public:
        World() = default;
        ~World() = default;

        void Update(float deltaTime);

        std::weak_ptr<GameObject> GetObject(GUID objectID) const;

        GUID CreateObject(const std::string& name = {});
        void DestroyObject(GUID objectID);

        template <typename... Component>
        auto GetAllObjectsWithComponents() const
        {
            return m_Registry.view<Component...>();
        }

    private:
        entt::registry m_Registry;
        std::unordered_map<GUID, std::shared_ptr<GameObject>> m_Objects;
    };
} // namespace DSM


#endif

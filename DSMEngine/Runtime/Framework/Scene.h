#pragma once
#ifndef __SCENE_H__
#define __SCENE_H__

#include <unordered_map>
#include <entt/entt.hpp>
#include "Runtime/Utils/Utils.h"



namespace DSM {
    class GameObject;

    using ObjectID = entt::entity;

    class Scene
    {
        friend class GameObject;
    public:
        Scene() = default;
        ~Scene();

        Scene(const Scene& other);
        Scene& operator=(const Scene& other);

        Scene(Scene&& other);
        Scene& operator=(Scene&& other);

        void Update(float deltaTime);
        void OnGUI();

        std::weak_ptr<GameObject> GetObjectByID(ObjectID objectID) const;
        
        ObjectID CreateObject(const std::string& name = {});
        void DestroyObject(ObjectID objectID);

        template <typename... Component>
        auto GetAllObjectsWithComponents() const
        {
            return m_Registry.view<Component...>();
        }

        template <typename Func> requires std::invocable<Func, entt::entity> && 
            std::same_as<std::invoke_result_t<Func, entt::entity>, void>
        void TraverseAllEntity(Func func) const
        {
            m_Registry.view<entt::entity>().each(func);
        }

        auto& GetAllObjects() { return m_Objects; }
        const auto& GetAllObjects() const { return m_Objects; }

    private:
        static void CopyScene(Scene& dest, const Scene& src);

    private:
        entt::registry m_Registry{};
        std::unordered_map<ObjectID, std::shared_ptr<GameObject>> m_Objects{};
    };
} // namespace DSM



#endif

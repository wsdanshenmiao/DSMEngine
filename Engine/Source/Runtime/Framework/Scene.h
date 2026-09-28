#pragma once
#ifndef __SCENE_H__
#define __SCENE_H__

#include <unordered_map>
#include <unordered_set>
#include <entt/entt.hpp>
#include "Runtime/Utils/Utils.h"



namespace DSM {
    class GameObject;

    using ObjectID = entt::entity;
	constexpr ObjectID c_InvalidObjectID = entt::null;

    class Scene
    {
        friend class GameObject;
    public:
        using ObjectCallback = std::function<void(std::shared_ptr<GameObject>)>;

        Scene() = default;
        ~Scene();

        Scene(const Scene& other);
        Scene& operator=(const Scene& other);

        Scene(Scene&& other);
        Scene& operator=(Scene&& other);

        void Update(float deltaTime);
        void OnGUI();

        bool IsDirty() const noexcept { return m_IsDirty; }
        void SetDirty(bool dirty) { m_IsDirty = dirty; }

        const std::string& GetSceneFilePath() const noexcept { return m_SceneFilePath; }
        void SetSceneFilePath(const std::string& path) { m_SceneFilePath = path; }

        const std::weak_ptr<GameObject> GetObjectByID(ObjectID objectID) const;
        std::weak_ptr<GameObject> GetObjectByID(ObjectID objectID);
        
        ObjectID CreateObject(const std::string& name = {});
        void DestroyObject(ObjectID objectID);

        template <typename... Component>
        auto GetObjectsWithComponents() const { return m_Registry.view<Component...>(); }
        template <typename... Component>
        auto GetObjectsWithComponents() { return m_Registry.view<Component...>(); }

        template <typename Func> requires std::invocable<Func, entt::entity> && 
            std::same_as<std::invoke_result_t<Func, entt::entity>, void>
        void TraverseAllEntity(Func func) const
        {
            m_Registry.view<entt::entity>().each(func);
        }

        auto& GetAllObjects() { return m_Objects; }
        const auto& GetAllObjects() const { return m_Objects; }
        const auto& GetRootObjects() const { return m_RootObjects; }

    private:
        static void CopyScene(Scene& dest, const Scene& src);

    private:
        entt::registry m_Registry{};

        std::unordered_map<ObjectID, std::shared_ptr<GameObject>> m_Objects{};
        std::unordered_set<std::shared_ptr<GameObject>> m_RootObjects{};

        std::string m_SceneFilePath{};

        bool m_IsDirty{ false };
    };
} // namespace DSM



#endif

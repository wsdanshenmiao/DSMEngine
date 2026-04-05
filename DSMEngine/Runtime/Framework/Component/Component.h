#pragma once
#ifndef __COMPONENT_H__
#define __COMPONENT_H__

#include <memory>

namespace DSM {
    class TransformComponent;
    class Light;
    class CameraComponent;
    class MeshRenderer;
    class NativeScript;
    class GameObject;

    class IComponent
    {
    public:
        IComponent(std::shared_ptr<GameObject> gameObject)
            : m_GameObject(gameObject) {}
        IComponent(const IComponent&) = default;
        IComponent& operator=(const IComponent&) = default;
        IComponent(IComponent&&) = default;
        IComponent& operator=(IComponent&&) = default;
        virtual ~IComponent() = default;

        bool IsDirty() const noexcept { return m_IsDirty; }
        void SetDirty(bool dirty) noexcept { m_IsDirty = dirty; }

    protected:
        std::weak_ptr<GameObject> m_GameObject{};
        bool m_IsDirty{true};
    };

    template <typename... Ts>
    struct type_list {};

    using AllComponents = type_list<
        TransformComponent,
        CameraComponent,
        MeshRenderer,
        Light,
        NativeScript>;
} // namespace DSM


#endif
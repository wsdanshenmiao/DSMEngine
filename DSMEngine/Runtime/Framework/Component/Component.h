#pragma once
#ifndef __TRANSFORMCOMPONENT_H__
#define __TRANSFORMCOMPONENT_H__

#include <variant>
#include <memory>

namespace DSM {
    class Transform;
    class Light;
    class Camera;
    class MeshRenderer;
    class NativeScript;
    class GameObject;

    class IComponent
    {
    public:
        IComponent() = default;
        IComponent(std::shared_ptr<GameObject> gameObject)
            : m_GameObject(gameObject) {}
        virtual ~IComponent() = default;

        bool IsDirty() const noexcept { return m_IsDirty; }
        void SetDirty(bool dirty) noexcept { m_IsDirty = dirty; }

    protected:
        std::weak_ptr<GameObject> m_GameObject{};
        bool m_IsDirty{true};
    };

    using AllComponents = std::variant<
        Transform,
        Camera,
        MeshRenderer,
        Light,
        NativeScript>;
} // namespace DSM


#endif
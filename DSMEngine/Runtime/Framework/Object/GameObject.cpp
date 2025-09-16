#include "GameObject.h"

namespace DSM {
    GameObject::GameObject(entt::entity handle, GUID id, World *world)
        : m_Handle(handle), m_ID(id), m_World(world)
    {

    }
}
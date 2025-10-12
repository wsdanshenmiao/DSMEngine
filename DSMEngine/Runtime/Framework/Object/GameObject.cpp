#include "GameObject.h"

namespace DSM {
    GameObject::GameObject(ObjectID handle, Scene *world)
        : m_Handle(handle), m_World(world)
    {

    }
}
#pragma once
#ifndef __TRANSFORMCOMPONENT_H__
#define __TRANSFORMCOMPONENT_H__

#include "Runtime/Math/Transform.h"

namespace DSM {
    class ScriptableObject;

    struct TagComponent
    {
        std::string tag;
    };

    struct NativeScriptComponent
    {
        std::unique_ptr<ScriptableObject> instance;
        std::function<std::unique_ptr<ScriptableObject>()> InstantiateScript;

        // 绑定初始化函数
        template <typename T>
        void BindInitFunc()
        {
            static_assert(std::is_base_of<ScriptableObject, T>::value, "T must be derived from ScriptableObject");
            InstantiateScript = []() { return std::make_unique<T>(); };
        }
    };
} // namespace DSM


#endif
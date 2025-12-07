#pragma once
#ifndef __TRANSFORMCOMPONENT_H__
#define __TRANSFORMCOMPONENT_H__

#include <variant>
#include "Runtime/Math/Transform.h"

namespace DSM {
    class ScriptableObject;

    struct TagComponent
    {
        std::string tag;
    };

    struct NativeScriptComponent
    {
        std::shared_ptr<ScriptableObject> instance;
        std::function<std::shared_ptr<ScriptableObject>()> InstantiateScript;

        // 绑定初始化函数
        template <typename T>
        void BindInitFunc()
        {
            static_assert(std::is_base_of<ScriptableObject, T>::value, "T must be derived from ScriptableObject");
            InstantiateScript = []() { return std::make_shared<T>(); };
        }
    };


    using AllComponents = std::variant<
        Math::Transform,
        TagComponent,
        NativeScriptComponent
    >;
} // namespace DSM


#endif
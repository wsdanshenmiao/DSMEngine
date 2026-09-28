#pragma once
#ifndef __NATIVESCRIPT_H__
#define __NATIVESCRIPT_H__

#include <memory>
#include <utility>

#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Framework/ScriptableObject.h"

namespace DSM {
    class NativeScript : public IComponent
    {
    public:
        NativeScript(std::shared_ptr<GameObject> gameObject)
            : IComponent(gameObject) {}

        bool IsEnabled() const noexcept { return m_Enabled; }
        void SetEnabled(bool enabled) noexcept { m_Enabled = enabled; m_IsDirty = true; }

        ScriptableObject* GetScript() const noexcept { return m_Script.get(); }
        void SetScript(std::unique_ptr<ScriptableObject> script) { m_Script = std::move(script); m_IsDirty = true; }
    
    private:
        bool m_Enabled = true;
        std::unique_ptr<ScriptableObject> m_Script{};
    };
}


#endif // !__NATIVESCRIPT_H__
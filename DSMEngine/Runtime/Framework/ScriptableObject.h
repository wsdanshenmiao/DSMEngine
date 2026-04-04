#pragma once
#ifndef __SCRIPTABLEOBJECT_H__
#define __SCRIPTABLEOBJECT_H__

#include "Runtime/Framework/Object/GameObject.h"

namespace DSM {
    class Scene;

    class ScriptableObject
    {
        friend class Scene;
    public:
        ScriptableObject()
        {
            SetEnabled(true);
        }
        virtual ~ScriptableObject()
        {
            OnDestroy();
        }

        bool IsEnabled() const noexcept { return m_Enabled; }
        void SetEnabled(bool enabled) noexcept
        {
            m_Enabled = enabled;
            if(enabled){
                OnEnable();
            }
            else{
                OnDisable();
            }
        }

    protected:
        virtual void Awake() {}
        virtual void OnEnable() {}
        virtual void Start() {}
        virtual void OnUpdate() {}
        virtual void OnGUI() {}
        virtual void OnDisable() {}
        virtual void OnDestroy() {}

    private:
        bool m_Enabled = false;
    };
}

#endif
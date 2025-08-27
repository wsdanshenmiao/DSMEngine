#pragma once
#ifndef __LAYER_H__
#define __LAYER_H__

#include "Event/Event.h"

class CpuTimer;

namespace DSM {

    class Layer
    {
    public:
        Layer(const std::string& name) : m_DebugName(name) {}
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(const CpuTimer& timer) {}
        virtual void OnGUIRender() {}
        virtual void OnEvent(Event& event) {}

        inline const std::string& GetName() const noexcept { return m_DebugName; }

    protected:
        std::string m_DebugName;
    };

} // namespace DSM 


#endif
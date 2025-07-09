#pragma once
#ifndef __LAYERSTACK_H__
#define __LAYERSTACK_H__

#include "Layer.h"

namespace DSM {
    class LayerStack
    {
    public:
        LayerStack() = default;
        ~LayerStack();

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        void PopLayer(Layer* layer);
        void PopOverlay(Layer* layer);

        decltype(auto) begin() noexcept { return m_Layers.begin(); }
        decltype(auto) begin() const noexcept { return m_Layers.begin(); }
        
        decltype(auto) rbegin() noexcept { return m_Layers.rbegin(); }
        decltype(auto) rbegin() const noexcept { return m_Layers.rbegin(); }
        
        decltype(auto) end() noexcept { return m_Layers.end(); }
        decltype(auto) end() const noexcept { return m_Layers.end(); }

        decltype(auto) rend() noexcept { return m_Layers.rend(); }
        decltype(auto) rend() const noexcept { return m_Layers.rend(); }

    private:
        std::vector<Layer*> m_Layers;
        uint32_t m_LayerInsertIndex = 0;
    };
} // namespace DSM 


#endif
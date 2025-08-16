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

        void PushLayer(std::shared_ptr<Layer> layer);
        void PushOverlay(std::shared_ptr<Layer> layer);

        void PopLayer(std::shared_ptr<Layer> layer);
        void PopOverlay(std::shared_ptr<Layer> layer);

        decltype(auto) begin() noexcept { return m_Layers.begin(); }
        decltype(auto) begin() const noexcept { return m_Layers.begin(); }
        
        decltype(auto) rbegin() noexcept { return m_Layers.rbegin(); }
        decltype(auto) rbegin() const noexcept { return m_Layers.rbegin(); }
        
        decltype(auto) end() noexcept { return m_Layers.end(); }
        decltype(auto) end() const noexcept { return m_Layers.end(); }

        decltype(auto) rend() noexcept { return m_Layers.rend(); }
        decltype(auto) rend() const noexcept { return m_Layers.rend(); }

    private:
        std::vector<std::shared_ptr<Layer>> m_Layers;
        uint32_t m_LayerInsertIndex = 0;
    };
} // namespace DSM 


#endif
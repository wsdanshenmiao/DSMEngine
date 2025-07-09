#include "LayerStack.h"


namespace DSM {
    LayerStack::~LayerStack()
    {
        for(auto& layer : m_Layers){
            layer->OnDetach();
            delete layer;
        }
    }

    void LayerStack::PushLayer(Layer *layer)
    {
        m_Layers.emplace(begin() + m_LayerInsertIndex, layer);
        m_LayerInsertIndex++;
    }

    void LayerStack::PushOverlay(Layer *layer)
    {
        m_Layers.emplace_back(layer);
    }

    void LayerStack::PopLayer(Layer *layer)
    {
        if(auto it = std::find(begin(), begin() + m_LayerInsertIndex, layer);
            it != begin() + m_LayerInsertIndex){
            layer->OnDetach();
            m_Layers.erase(it);
            m_LayerInsertIndex--;
        }
    }

    void LayerStack::PopOverlay(Layer *layer)
    {
        if(auto it = std::find(begin() + m_LayerInsertIndex, end(), layer);
            it != end()){
            layer->OnDetach();
            m_Layers.erase(it);
        }
    }

} // namespace DSM 
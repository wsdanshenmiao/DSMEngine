#include "LayerStack.h"


namespace DSM {
    LayerStack::~LayerStack()
    {
        for(auto& layer : m_Layers){
            layer->OnDetach();
        }
    }

    void LayerStack::PushLayer(std::shared_ptr<Layer> layer)
    {
        m_Layers.emplace(begin() + m_LayerInsertIndex, layer);
        m_LayerInsertIndex++;
        layer->OnAttach();
    }

    void LayerStack::PushOverlay(std::shared_ptr<Layer> layer)
    {
        m_Layers.emplace_back(layer);
        layer->OnAttach();
    }

    void LayerStack::PopLayer(std::shared_ptr<Layer> layer)
    {
        if(auto it = std::find(begin(), begin() + m_LayerInsertIndex, layer);
            it != begin() + m_LayerInsertIndex){
            layer->OnDetach();
            m_Layers.erase(it);
            m_LayerInsertIndex--;
        }
    }

    void LayerStack::PopOverlay(std::shared_ptr<Layer> layer)
    {
        if(auto it = std::find(begin() + m_LayerInsertIndex, end(), layer);
            it != end()){
            layer->OnDetach();
            m_Layers.erase(it);
        }
    }

} // namespace DSM 
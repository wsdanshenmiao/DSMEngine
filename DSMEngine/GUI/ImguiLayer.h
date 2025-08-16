#pragma once
#ifndef __IMGUILAYER_H__
#define __IMGUILAYER_H__

#include "Core/Layer.h"

namespace DSM {
    struct IDevice;

    class ImguiLayer : public Layer
    {
    public:
        ImguiLayer(IDevice* device) : Layer("ImguiLayer") {}

        void OnAttach() override;
        void OnDetach() override;
        void OnEvent(Event& event) override;

        void Begin();
        void End();

        static ImguiLayer* Create();
    };
    
} // namespace DSM 

#endif
#pragma once
#ifndef __IMGUILAYER_H__
#define __IMGUILAYER_H__

#include "Core/Layer.h"

namespace DSM {
    struct IDevice;
    class Window;
    struct IFramebuffer;

    class ImguiLayer : public Layer
    {
    public:
        ImguiLayer(IDevice* device, const Window& window);

        void OnAttach() override;
        void OnDetach() override;

        void Begin(IFramebuffer* fb);
        void End(IFramebuffer* fb);

    private:
        IDevice* m_Device{};
        const Window& m_Window;
    };
    
} // namespace DSM 

#endif
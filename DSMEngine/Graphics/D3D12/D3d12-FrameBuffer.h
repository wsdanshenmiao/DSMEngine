#pragma once
#ifndef __D3D12_FRAMEBUFFER_H__
#define __D3D12_FRAMEBUFFER_H__

#include "D3D12-Device.h"

namespace DSM::D3D12
{
    class Framebuffer : public IFramebuffer
    {
    public:
        Framebuffer(DeviceResources& resources, FramebufferDesc desc)
            : m_Resources(resources), m_Desc(std::move(desc)), m_Info(m_Desc) {}

        ~Framebuffer()
        {
            // 释放描述符
            for(const auto& rtv : RTVs){
                m_Resources.renderTargetViewHeap.ReleaseDescriptor(rtv);
            }
            if(m_Desc.depthAttachment.Valid()){
                m_Resources.depthStencilViewHeap.ReleaseDescriptor(DSV);
            }
        }

        const FramebufferDesc& GetDesc() const override { return m_Desc; }
        const FramebufferInfo& GetFramebufferInfo() const override { return m_Info; }

    public:
        // 所有的 RenderTarget 和 DepthStencil
        StaticVector<TextureHandle, c_MaxRenderTargets + 1> textures;
        StaticVector<uint32_t, c_MaxRenderTargets> RTVs;
        uint32_t DSV;
        uint32_t rtWidth;
        uint32_t rtHeight;

    private:
        DeviceResources& m_Resources;
        const FramebufferDesc m_Desc;
        FramebufferInfo m_Info;
    };
    
}

#endif // __D3D12_FRAMEBUFFER_H__
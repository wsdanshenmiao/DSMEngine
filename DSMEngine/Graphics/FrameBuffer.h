#ifndef __FRAMEBUFFER_H__
#define __FRAMEBUFFER_H__

#include "Texture.h"

namespace DSM {
    
    struct FramebufferAttachment
    {
        ITexture* texture = nullptr;
        TextureSubresourceSet subresources = TextureSubresourceSet(0, 1, 0, 1);
        Format format = Format::UNKNOWN;
        bool isReadOnly = false;
        
        constexpr FramebufferAttachment& SetTexture(ITexture* t) { texture = t; return *this; }
        constexpr FramebufferAttachment& SetSubresources(TextureSubresourceSet value) { subresources = value; return *this; }
        constexpr FramebufferAttachment& SetArraySlice(uint32_t index) { subresources.baseArraySlice = index; subresources.numArraySlices = 1; return *this; }
        constexpr FramebufferAttachment& SetArraySliceRange(uint32_t index, uint32_t count) { subresources.baseArraySlice = index; subresources.numArraySlices = count; return *this; }
        constexpr FramebufferAttachment& SetMipLevel(uint32_t level) { subresources.baseMipLevel = level; subresources.numMipLevels = 1; return *this; }
        constexpr FramebufferAttachment& SetFormat(Format f) { format = f; return *this; }
        constexpr FramebufferAttachment& SetReadOnly(bool ro) { isReadOnly = ro; return *this; }

        [[nodiscard]] bool Valid() const { return texture != nullptr; }
    };

    struct FramebufferDesc
    {
        StaticVector<FramebufferAttachment, c_MaxRenderTargets> colorAttachments;
        FramebufferAttachment depthAttachment;
        FramebufferAttachment shadingRateAttachment;

        FramebufferDesc& AddColorAttachment(const FramebufferAttachment& a) { colorAttachments.push_back(a); return *this; }
        FramebufferDesc& AddColorAttachment(ITexture* texture) { colorAttachments.push_back(FramebufferAttachment().SetTexture(texture)); return *this; }
        FramebufferDesc& AddColorAttachment(ITexture* texture, TextureSubresourceSet subresources) { colorAttachments.push_back(FramebufferAttachment().SetTexture(texture).SetSubresources(subresources)); return *this; }
        FramebufferDesc& SetDepthAttachment(const FramebufferAttachment& d) { depthAttachment = d; return *this; }
        FramebufferDesc& SetDepthAttachment(ITexture* texture) { depthAttachment = FramebufferAttachment().SetTexture(texture); return *this; }
        FramebufferDesc& SetDepthAttachment(ITexture* texture, TextureSubresourceSet subresources) { depthAttachment = FramebufferAttachment().SetTexture(texture).SetSubresources(subresources); return *this; }
        FramebufferDesc& SetShadingRateAttachment(const FramebufferAttachment& d) { shadingRateAttachment = d; return *this; }
        FramebufferDesc& SetShadingRateAttachment(ITexture* texture) { shadingRateAttachment = FramebufferAttachment().SetTexture(texture); return *this; }
        FramebufferDesc& SetShadingRateAttachment(ITexture* texture, TextureSubresourceSet subresources) { shadingRateAttachment = FramebufferAttachment().SetTexture(texture).SetSubresources(subresources); return *this; }
    };

    struct FramebufferInfo
    {
        StaticVector<Format, c_MaxRenderTargets> colorFormats;
        Format depthFormat = Format::UNKNOWN;
        uint32_t sampleCount = 1;
        uint32_t sampleQuality = 0;
        uint32_t width = 0;
        uint32_t height = 0;

        FramebufferInfo() = default;
        FramebufferInfo(const FramebufferDesc& desc)
        {
            for(const auto& attachment : desc.colorAttachments){
                auto format = (attachment.format == Format::UNKNOWN && attachment.Valid()) ?
                    attachment.texture->GetDesc().format : attachment.format;
                colorFormats.push_back(format);
            }
            if(desc.depthAttachment.Valid()){
                const auto& texDesc = desc.depthAttachment.texture->GetDesc();
                depthFormat = texDesc.format;
                sampleCount = texDesc.sampleCount;
                sampleQuality = texDesc.sampleQuality;
                width = std::max(texDesc.width >> desc.depthAttachment.subresources.baseMipLevel, 1u);
                height = std::max(texDesc.height >> desc.depthAttachment.subresources.baseMipLevel, 1u);
            }
            else if(!desc.colorAttachments.empty() && desc.colorAttachments[0].Valid()){
                const auto& texDesc = desc.colorAttachments[0].texture->GetDesc();
                sampleCount = texDesc.sampleCount;
                sampleQuality = texDesc.sampleQuality;
                width = std::max(texDesc.width >> desc.colorAttachments[0].subresources.baseMipLevel, 1u);
                height = std::max(texDesc.height >> desc.colorAttachments[0].subresources.baseMipLevel, 1u);
            }
        }
        
        bool operator==(const FramebufferInfo& other) const = default;
    };

    struct IFramebuffer : public IResource 
    {
        [[nodiscard]] virtual const FramebufferDesc& GetDesc() const = 0;
        [[nodiscard]] virtual const FramebufferInfo& GetFramebufferInfo() const = 0;
    };
    using FramebufferHandle = RefPtr<IFramebuffer>;
    
} // namespace DSM 



template<>
struct std::hash<DSM::FramebufferInfo>
{
    std::size_t operator()(const DSM::FramebufferInfo& s)
    {
        std::size_t hash = 0;
        for(const auto& format : s.colorFormats){
            hash = DSM::Utility::HashCombine(hash, format);
        }
        hash = DSM::Utility::HashCombine(hash, s.depthFormat);
        hash = DSM::Utility::HashCombine(hash, s.sampleCount);
        hash = DSM::Utility::HashCombine(hash, s.sampleQuality);
        return hash;
    }
};

#endif
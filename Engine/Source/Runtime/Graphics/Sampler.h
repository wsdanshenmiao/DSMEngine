#ifndef __SAMPLER_H__
#define __SAMPLER_H__

#include "GraphicsCommon.h"

namespace DSM {

    enum class SamplerAddressMode : uint8_t
    {
        Clamp,
        Wrap,
        Border,
        Mirror,
        MirrorOnce,
    };

    enum class SamplerReductionType : uint8_t
    {
        Standard,
        Comparison,
        Minimum,
        Maximum
    };

    struct SamplerDesc
    {
        Color borderColor = 1.f;
        float maxAnisotropy = 1.f;
        float mipBias = 0.f;

        // 分别表示放大缩小以及采样 mipmap 是使用点采样还是线性插值，为 true 为线性插值， false 为点采样
        bool minFilter = true;
        bool magFilter = true;
        bool mipFilter = true;

        SamplerAddressMode addressU = SamplerAddressMode::Clamp;
        SamplerAddressMode addressV = SamplerAddressMode::Clamp;
        SamplerAddressMode addressW = SamplerAddressMode::Clamp;

        SamplerReductionType reductionType = SamplerReductionType::Standard;
        ComparisonFunc comparisonFunc = ComparisonFunc::LessOrEqual;

        SamplerDesc& SetBorderColor(const Color& color) { borderColor = color; return *this; }
        SamplerDesc& SetMaxAnisotropy(float value) { maxAnisotropy = value; return *this; }
        SamplerDesc& SetMipBias(float value) { mipBias = value; return *this; }
        SamplerDesc& SetMinFilter(bool enable) { minFilter = enable; return *this; }
        SamplerDesc& SetMagFilter(bool enable) { magFilter = enable; return *this; }
        SamplerDesc& SetMipFilter(bool enable) { mipFilter = enable; return *this; }
        SamplerDesc& SetAllFilters(bool enable) { minFilter = magFilter = mipFilter = enable; return *this; }
        SamplerDesc& SetAddressU(SamplerAddressMode mode) { addressU = mode; return *this; }
        SamplerDesc& SetAddressV(SamplerAddressMode mode) { addressV = mode; return *this; }
        SamplerDesc& SetAddressW(SamplerAddressMode mode) { addressW = mode; return *this; }
        SamplerDesc& SetAllAddressModes(SamplerAddressMode mode) { addressU = addressV = addressW = mode; return *this; }
        SamplerDesc& SetReductionType(SamplerReductionType type) { reductionType = type; return *this; }
        SamplerDesc& SetComparisonFunc(ComparisonFunc func) { comparisonFunc = func; return *this; }
    };

    class ISampler : public IResource
    {
    public:
        [[nodiscard]] virtual const SamplerDesc& GetDesc() const = 0;
    };
    using SamplerHandle = RefPtr<ISampler>;

    
} // namespace DSM 

#endif
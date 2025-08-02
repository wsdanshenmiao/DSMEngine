#pragma once
#ifndef __D3D12_SAMPLER_H__
#define __D3D12_SAMPLER_H__

#include "D3D12Common.h"


namespace DSM::D3D12 {
    class Sampler : public ISampler
    {
    public:
        Sampler(const Context& context, SamplerDesc desc)
            :m_Context(context), m_Desc(std::move(desc)){}

        const SamplerDesc& GetDesc() const override { return m_Desc; }

        void CreateDescriptor(size_t descriptor)
        {
            D3D12_SAMPLER_DESC desc{};
            desc.AddressU = ConvertAddressMode(m_Desc.addressU);
            desc.AddressV = ConvertAddressMode(m_Desc.addressV);
            desc.AddressW = ConvertAddressMode(m_Desc.addressW);

            uint32_t reductionType = ConvertReductionType(m_Desc.reductionType);

            if(m_Desc.maxAnisotropy > 1){
                desc.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(reductionType);
            }
            else{
                desc.Filter = D3D12_ENCODE_BASIC_FILTER(
                    m_Desc.minFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                    m_Desc.magFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                    m_Desc.mipFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
                    reductionType);
            } 

            desc.ComparisonFunc = m_Desc.reductionType == SamplerReductionType::Comparison ? 
                D3D12_COMPARISON_FUNC_LESS : D3D12_COMPARISON_FUNC_NEVER;
            desc.MaxAnisotropy = std::max(0u, (UINT)m_Desc.maxAnisotropy);
            desc.MaxLOD = D3D12_FLOAT32_MAX;
            desc.MinLOD = 0;
            desc.MipLODBias = m_Desc.mipBias;

            desc.BorderColor[0] = m_Desc.borderColor.r;
            desc.BorderColor[1] = m_Desc.borderColor.g;
            desc.BorderColor[2] = m_Desc.borderColor.b;
            desc.BorderColor[3] = m_Desc.borderColor.a;

            m_Context.m_Device->CreateSampler(&desc, {descriptor});
        }

        static D3D12_TEXTURE_ADDRESS_MODE ConvertAddressMode(SamplerAddressMode mode)
        {
            switch (mode)
            {
            case SamplerAddressMode::Clamp:
                return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            case SamplerAddressMode::Wrap:
                return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            case SamplerAddressMode::Border:
                return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            case SamplerAddressMode::Mirror:
                return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            case SamplerAddressMode::MirrorOnce:
                return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
            default:
                assert(!"Invalid address mode.");
                return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
        }

        static D3D12_FILTER_REDUCTION_TYPE ConvertReductionType(SamplerReductionType reductionType)
        {
            switch (reductionType)
            {
            case SamplerReductionType::Standard:
                return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
            case SamplerReductionType::Comparison:
                return D3D12_FILTER_REDUCTION_TYPE_COMPARISON;
            case SamplerReductionType::Minimum:
                return D3D12_FILTER_REDUCTION_TYPE_MINIMUM;
            case SamplerReductionType::Maximum:
                return D3D12_FILTER_REDUCTION_TYPE_MAXIMUM;
            default:
                assert(!"Invalid sampler reduction type.");
                return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
            }
        }

    private:
        const Context& m_Context;
        const SamplerDesc m_Desc{};
    };

} // namespace DSM 

#endif

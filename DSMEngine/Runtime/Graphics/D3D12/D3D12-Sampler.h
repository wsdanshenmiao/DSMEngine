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
                ConvertComparisonFunc(m_Desc.comparisonFunc) : D3D12_COMPARISON_FUNC_NEVER;
            desc.MaxAnisotropy = std::max(0u, (UINT)m_Desc.maxAnisotropy);
            desc.MaxLOD = D3D12_FLOAT32_MAX;
            desc.MinLOD = 0;
            desc.MipLODBias = m_Desc.mipBias;

            desc.BorderColor[0] = m_Desc.borderColor.r;
            desc.BorderColor[1] = m_Desc.borderColor.g;
            desc.BorderColor[2] = m_Desc.borderColor.b;
            desc.BorderColor[3] = m_Desc.borderColor.a;

            m_Context.device->CreateSampler(&desc, {descriptor});
        }

    private:
        const Context& m_Context;
        const SamplerDesc m_Desc{};
    };

} // namespace DSM 

#endif

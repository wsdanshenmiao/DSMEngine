#pragma once
#ifndef __D3D12COMMON_H__
#define __D3D12COMMON_H__

#include "Graphics/D3D12.h"
#include <unordered_map>

namespace DSM{    
    constexpr uint32_t c_InvalidRootParameterIndex = ~0u;
    constexpr uint32_t c_InvalidDescriptorIndex = ~0u;

    // 根据输入的格式输出不同视图的格式
    struct DxgiFormatMapping
    {
        Format abstractFormat;
        DXGI_FORMAT resourceFormat;
        DXGI_FORMAT srvFormat;
        DXGI_FORMAT rtvFormat;
    };

    // Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
    static const DxgiFormatMapping c_FormatMappings[] = {
        { Format::UNKNOWN,              DXGI_FORMAT_UNKNOWN,                DXGI_FORMAT_UNKNOWN,                  DXGI_FORMAT_UNKNOWN                },

        { Format::R8_UINT,              DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_UINT,                  DXGI_FORMAT_R8_UINT                },
        { Format::R8_SINT,              DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_SINT,                  DXGI_FORMAT_R8_SINT                },
        { Format::R8_UNORM,             DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_UNORM,                 DXGI_FORMAT_R8_UNORM               },
        { Format::R8_SNORM,             DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_SNORM,                 DXGI_FORMAT_R8_SNORM               },
        { Format::RG8_UINT,             DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_UINT,                DXGI_FORMAT_R8G8_UINT              },
        { Format::RG8_SINT,             DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_SINT,                DXGI_FORMAT_R8G8_SINT              },
        { Format::RG8_UNORM,            DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_UNORM,               DXGI_FORMAT_R8G8_UNORM             },
        { Format::RG8_SNORM,            DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_SNORM,               DXGI_FORMAT_R8G8_SNORM             },
        { Format::R16_UINT,             DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UINT,                 DXGI_FORMAT_R16_UINT               },
        { Format::R16_SINT,             DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_SINT,                 DXGI_FORMAT_R16_SINT               },
        { Format::R16_UNORM,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UNORM,                DXGI_FORMAT_R16_UNORM              },
        { Format::R16_SNORM,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_SNORM,                DXGI_FORMAT_R16_SNORM              },
        { Format::R16_FLOAT,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_FLOAT,                DXGI_FORMAT_R16_FLOAT              },
        { Format::BGRA4_UNORM,          DXGI_FORMAT_B4G4R4A4_UNORM,         DXGI_FORMAT_B4G4R4A4_UNORM,           DXGI_FORMAT_B4G4R4A4_UNORM         },
        { Format::B5G6R5_UNORM,         DXGI_FORMAT_B5G6R5_UNORM,           DXGI_FORMAT_B5G6R5_UNORM,             DXGI_FORMAT_B5G6R5_UNORM           },
        { Format::B5G5R5A1_UNORM,       DXGI_FORMAT_B5G5R5A1_UNORM,         DXGI_FORMAT_B5G5R5A1_UNORM,           DXGI_FORMAT_B5G5R5A1_UNORM         },
        { Format::RGBA8_UINT,           DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UINT,            DXGI_FORMAT_R8G8B8A8_UINT          },
        { Format::RGBA8_SINT,           DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_SINT,            DXGI_FORMAT_R8G8B8A8_SINT          },
        { Format::RGBA8_UNORM,          DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UNORM,           DXGI_FORMAT_R8G8B8A8_UNORM         },
        { Format::RGBA8_SNORM,          DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_SNORM,           DXGI_FORMAT_R8G8B8A8_SNORM         },
        { Format::BGRA8_UNORM,          DXGI_FORMAT_B8G8R8A8_TYPELESS,      DXGI_FORMAT_B8G8R8A8_UNORM,           DXGI_FORMAT_B8G8R8A8_UNORM         },
        { Format::SRGBA8_UNORM,         DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB    },
        { Format::SBGRA8_UNORM,         DXGI_FORMAT_B8G8R8A8_TYPELESS,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB    },
        { Format::R10G10B10A2_UNORM,    DXGI_FORMAT_R10G10B10A2_TYPELESS,   DXGI_FORMAT_R10G10B10A2_UNORM,        DXGI_FORMAT_R10G10B10A2_UNORM      },
        { Format::R11G11B10_FLOAT,      DXGI_FORMAT_R11G11B10_FLOAT,        DXGI_FORMAT_R11G11B10_FLOAT,          DXGI_FORMAT_R11G11B10_FLOAT        },
        { Format::RG16_UINT,            DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_UINT,              DXGI_FORMAT_R16G16_UINT            },
        { Format::RG16_SINT,            DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_SINT,              DXGI_FORMAT_R16G16_SINT            },
        { Format::RG16_UNORM,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_UNORM,             DXGI_FORMAT_R16G16_UNORM           },
        { Format::RG16_SNORM,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_SNORM,             DXGI_FORMAT_R16G16_SNORM           },
        { Format::RG16_FLOAT,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_FLOAT,             DXGI_FORMAT_R16G16_FLOAT           },
        { Format::R32_UINT,             DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_UINT,                 DXGI_FORMAT_R32_UINT               },
        { Format::R32_SINT,             DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_SINT,                 DXGI_FORMAT_R32_SINT               },
        { Format::R32_FLOAT,            DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_FLOAT,                DXGI_FORMAT_R32_FLOAT              },
        { Format::RGBA16_UINT,          DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_UINT,        DXGI_FORMAT_R16G16B16A16_UINT      },
        { Format::RGBA16_SINT,          DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_SINT,        DXGI_FORMAT_R16G16B16A16_SINT      },
        { Format::RGBA16_FLOAT,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_FLOAT,       DXGI_FORMAT_R16G16B16A16_FLOAT     },
        { Format::RGBA16_UNORM,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_UNORM,       DXGI_FORMAT_R16G16B16A16_UNORM     },
        { Format::RGBA16_SNORM,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_SNORM,       DXGI_FORMAT_R16G16B16A16_SNORM     },
        { Format::RG32_UINT,            DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_UINT,              DXGI_FORMAT_R32G32_UINT            },
        { Format::RG32_SINT,            DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_SINT,              DXGI_FORMAT_R32G32_SINT            },
        { Format::RG32_FLOAT,           DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_FLOAT,             DXGI_FORMAT_R32G32_FLOAT           },
        { Format::RGB32_UINT,           DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_UINT,           DXGI_FORMAT_R32G32B32_UINT         },
        { Format::RGB32_SINT,           DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_SINT,           DXGI_FORMAT_R32G32B32_SINT         },
        { Format::RGB32_FLOAT,          DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_FLOAT,          DXGI_FORMAT_R32G32B32_FLOAT        },
        { Format::RGBA32_UINT,          DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_UINT,        DXGI_FORMAT_R32G32B32A32_UINT      },
        { Format::RGBA32_SINT,          DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_SINT,        DXGI_FORMAT_R32G32B32A32_SINT      },
        { Format::RGBA32_FLOAT,         DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_FLOAT,       DXGI_FORMAT_R32G32B32A32_FLOAT     },

        { Format::D16,                  DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UNORM,                DXGI_FORMAT_D16_UNORM              },
        { Format::D24S8,                DXGI_FORMAT_R24G8_TYPELESS,         DXGI_FORMAT_R24_UNORM_X8_TYPELESS,    DXGI_FORMAT_D24_UNORM_S8_UINT      },
        { Format::X24G8_UINT,           DXGI_FORMAT_R24G8_TYPELESS,         DXGI_FORMAT_X24_TYPELESS_G8_UINT,     DXGI_FORMAT_D24_UNORM_S8_UINT      },
        { Format::D32,                  DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_FLOAT,                DXGI_FORMAT_D32_FLOAT              },
        { Format::D32S8,                DXGI_FORMAT_R32G8X24_TYPELESS,      DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, DXGI_FORMAT_D32_FLOAT_S8X24_UINT   },
        { Format::X32G8_UINT,           DXGI_FORMAT_R32G8X24_TYPELESS,      DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,  DXGI_FORMAT_D32_FLOAT_S8X24_UINT   },
        
        { Format::BC1_UNORM,            DXGI_FORMAT_BC1_TYPELESS,           DXGI_FORMAT_BC1_UNORM,                DXGI_FORMAT_BC1_UNORM              },
        { Format::BC1_UNORM_SRGB,       DXGI_FORMAT_BC1_TYPELESS,           DXGI_FORMAT_BC1_UNORM_SRGB,           DXGI_FORMAT_BC1_UNORM_SRGB         },
        { Format::BC2_UNORM,            DXGI_FORMAT_BC2_TYPELESS,           DXGI_FORMAT_BC2_UNORM,                DXGI_FORMAT_BC2_UNORM              },
        { Format::BC2_UNORM_SRGB,       DXGI_FORMAT_BC2_TYPELESS,           DXGI_FORMAT_BC2_UNORM_SRGB,           DXGI_FORMAT_BC2_UNORM_SRGB         },
        { Format::BC3_UNORM,            DXGI_FORMAT_BC3_TYPELESS,           DXGI_FORMAT_BC3_UNORM,                DXGI_FORMAT_BC3_UNORM              },
        { Format::BC3_UNORM_SRGB,       DXGI_FORMAT_BC3_TYPELESS,           DXGI_FORMAT_BC3_UNORM_SRGB,           DXGI_FORMAT_BC3_UNORM_SRGB         },
        { Format::BC4_UNORM,            DXGI_FORMAT_BC4_TYPELESS,           DXGI_FORMAT_BC4_UNORM,                DXGI_FORMAT_BC4_UNORM              },
        { Format::BC4_SNORM,            DXGI_FORMAT_BC4_TYPELESS,           DXGI_FORMAT_BC4_SNORM,                DXGI_FORMAT_BC4_SNORM              },
        { Format::BC5_UNORM,            DXGI_FORMAT_BC5_TYPELESS,           DXGI_FORMAT_BC5_UNORM,                DXGI_FORMAT_BC5_UNORM              },
        { Format::BC5_SNORM,            DXGI_FORMAT_BC5_TYPELESS,           DXGI_FORMAT_BC5_SNORM,                DXGI_FORMAT_BC5_SNORM              },
        { Format::BC6H_UFLOAT,          DXGI_FORMAT_BC6H_TYPELESS,          DXGI_FORMAT_BC6H_UF16,                DXGI_FORMAT_BC6H_UF16              },
        { Format::BC6H_SFLOAT,          DXGI_FORMAT_BC6H_TYPELESS,          DXGI_FORMAT_BC6H_SF16,                DXGI_FORMAT_BC6H_SF16              },
        { Format::BC7_UNORM,            DXGI_FORMAT_BC7_TYPELESS,           DXGI_FORMAT_BC7_UNORM,                DXGI_FORMAT_BC7_UNORM              },
        { Format::BC7_UNORM_SRGB,       DXGI_FORMAT_BC7_TYPELESS,           DXGI_FORMAT_BC7_UNORM_SRGB,           DXGI_FORMAT_BC7_UNORM_SRGB         },
    };

    static const DxgiFormatMapping& GetDxgiFormatMapping(Format abstractFormat)
    {
        static_assert(sizeof(c_FormatMappings) / sizeof(DxgiFormatMapping) == size_t(Format::COUNT), 
            "The format mapping table doesn't have the right number of elements");

        const DxgiFormatMapping& mapping = c_FormatMappings[uint32_t(abstractFormat)];
        assert(mapping.abstractFormat == abstractFormat);
        return mapping;
    }



    namespace D3D12{     
        class Buffer;
        class DescriptorHeap;
        class RootSignature;


        struct Context
        {
            RefPtr<ID3D12Device> device;
            RefPtr<ID3D12Device2> device2;
            RefPtr<ID3D12Device5> device5;
            RefPtr<ID3D12Device8> device8;

            RefPtr<ID3D12CommandSignature> drawIndirectSignature;
            RefPtr<ID3D12CommandSignature> drawIndexedIndirectSignature;
            RefPtr<ID3D12CommandSignature> dispatchIndirectSignature;
            RefPtr<ID3D12QueryHeap> timerQueryHeap;
            RefPtr<Buffer> timerQueryResolveBuffer;

            bool logBufferLifetime = false;
            IMessageCallback* messageCallback = nullptr;
            
            void Error(const std::string& message) const
            {
                messageCallback->Message(MessageSeverity::Error, message.c_str());
            }

            void Info(const std::string& message) const
            {
                messageCallback->Message(MessageSeverity::Info, message.c_str());
            }
        };

        class InputLayout : public IInputLayout
        {
        public:
            uint32_t GetNumAttributes() const override { return attributes.size(); }
            const VertexAttributeDesc* GetAttributeDesc(uint32_t index) const override
            {
                return index < uint32_t(attributes.size()) ? &attributes[index] : nullptr;
            }

            std::vector<VertexAttributeDesc> attributes;
            std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;

            // 一个属性绑定一个槽
            std::unordered_map<uint32_t, uint32_t> elementStride;
        };

        struct DX12_ViewportState
        {
            UINT numViewports = 0;
            D3D12_VIEWPORT viewports[16] = {};
            UINT numScissorRects = 0;
            D3D12_RECT scissorRects[16] = {};
        };

        
        
        //////////////////////////////////////////////////////////////////////////
        // Convert Custom Structs/Enums to D3D12 Structs/Enums
        //////////////////////////////////////////////////////////////////////////
        static D3D12_RESOURCE_STATES ConvertResourceStates(const ResourceStates& state)
        {
            if (state == ResourceStates::Common)
                return D3D12_RESOURCE_STATE_COMMON;

            D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON; // also 0

            if (HasFlags(state, ResourceStates::ConstantBuffer)) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            if (HasFlags(state, ResourceStates::VertexBuffer)) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            if (HasFlags(state, ResourceStates::IndexBuffer)) result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
            if (HasFlags(state, ResourceStates::IndirectArgument)) result |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            if (HasFlags(state, ResourceStates::ShaderResource)) result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            if (HasFlags(state, ResourceStates::UnorderedAccess)) result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            if (HasFlags(state, ResourceStates::RenderTarget)) result |= D3D12_RESOURCE_STATE_RENDER_TARGET;
            if (HasFlags(state, ResourceStates::DepthWrite)) result |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
            if (HasFlags(state, ResourceStates::DepthRead)) result |= D3D12_RESOURCE_STATE_DEPTH_READ;
            if (HasFlags(state, ResourceStates::StreamOut)) result |= D3D12_RESOURCE_STATE_STREAM_OUT;
            if (HasFlags(state, ResourceStates::CopyDest)) result |= D3D12_RESOURCE_STATE_COPY_DEST;
            if (HasFlags(state, ResourceStates::CopySource)) result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
            if (HasFlags(state, ResourceStates::ResolveDest)) result |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
            if (HasFlags(state, ResourceStates::ResolveSource)) result |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            if (HasFlags(state, ResourceStates::Present)) result |= D3D12_RESOURCE_STATE_PRESENT;
            if (HasFlags(state, ResourceStates::AccelerationStructrue)) result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            if (HasFlags(state, ResourceStates::ShadingRateSurface)) result |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;

            return result;
        }
            
        static D3D12_SHADER_VISIBILITY ConvertShaderStage(ShaderType s)
        {
            switch (s) {
            case ShaderType::Vertex:
                return D3D12_SHADER_VISIBILITY_VERTEX;
            case ShaderType::Hull:
                return D3D12_SHADER_VISIBILITY_HULL;
            case ShaderType::Domain:
                return D3D12_SHADER_VISIBILITY_DOMAIN;
            case ShaderType::Geometry:
                return D3D12_SHADER_VISIBILITY_GEOMETRY;
            case ShaderType::Pixel:
                return D3D12_SHADER_VISIBILITY_PIXEL;
            case ShaderType::Amplification:
                return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
            case ShaderType::Mesh:
                return D3D12_SHADER_VISIBILITY_MESH;
            default:
                return D3D12_SHADER_VISIBILITY_ALL;
            }
        }

        static D3D12_BLEND ConvertBlendValue(BlendFactor value)
        {
            switch (value) {
            case BlendFactor::Zero:
                return D3D12_BLEND_ZERO;
            case BlendFactor::One:
                return D3D12_BLEND_ONE;
            case BlendFactor::SrcColor:
                return D3D12_BLEND_SRC_COLOR;
            case BlendFactor::InvSrcColor:
                return D3D12_BLEND_INV_SRC_COLOR;
            case BlendFactor::SrcAlpha:
                return D3D12_BLEND_SRC_ALPHA;
            case BlendFactor::InvSrcAlpha:
                return D3D12_BLEND_INV_SRC_ALPHA;
            case BlendFactor::DstAlpha:
                return D3D12_BLEND_DEST_ALPHA;
            case BlendFactor::InvDstAlpha:
                return D3D12_BLEND_INV_DEST_ALPHA;
            case BlendFactor::DstColor:
                return D3D12_BLEND_DEST_COLOR;
            case BlendFactor::InvDstColor:
                return D3D12_BLEND_INV_DEST_COLOR;
            case BlendFactor::SrcAlphaSaturate:
                return D3D12_BLEND_SRC_ALPHA_SAT;
            case BlendFactor::ConstantColor:
                return D3D12_BLEND_BLEND_FACTOR;
            case BlendFactor::InvConstantColor:
                return D3D12_BLEND_INV_BLEND_FACTOR;
            case BlendFactor::Src1Color:
                return D3D12_BLEND_SRC1_COLOR;
            case BlendFactor::InvSrc1Color:
                return D3D12_BLEND_INV_SRC1_COLOR;
            case BlendFactor::Src1Alpha:
                return D3D12_BLEND_SRC1_ALPHA;
            case BlendFactor::InvSrc1Alpha:
                return D3D12_BLEND_INV_SRC1_ALPHA;
            default:
                assert(!"Invalid blend factor.");
                return D3D12_BLEND_ZERO;
            }
        }

        static D3D12_BLEND_OP ConvertBlendOp(BlendOp value)
        {
            switch (value) {
            case BlendOp::Add:
                return D3D12_BLEND_OP_ADD;
            case BlendOp::Subtract:
                return D3D12_BLEND_OP_SUBTRACT;
            case BlendOp::ReverseSubtract:
                return D3D12_BLEND_OP_REV_SUBTRACT;
            case BlendOp::Min:
                return D3D12_BLEND_OP_MIN;
            case BlendOp::Max:
                return D3D12_BLEND_OP_MAX;
            default:
                assert(!"Invalid blend op.");
                return D3D12_BLEND_OP_ADD;
            }
        }

        static D3D12_STENCIL_OP ConvertStencilOp(StencilOp value)
        {
            switch (value) {
            case StencilOp::Keep:
                return D3D12_STENCIL_OP_KEEP;
            case StencilOp::Zero:
                return D3D12_STENCIL_OP_ZERO;
            case StencilOp::Replace:
                return D3D12_STENCIL_OP_REPLACE;
            case StencilOp::IncrementAndClamp:
                return D3D12_STENCIL_OP_INCR_SAT;
            case StencilOp::DecrementAndClamp:
                return D3D12_STENCIL_OP_DECR_SAT;
            case StencilOp::Invert:
                return D3D12_STENCIL_OP_INVERT;
            case StencilOp::IncrementAndWrap:
                return D3D12_STENCIL_OP_INCR;
            case StencilOp::DecrementAndWrap:
                return D3D12_STENCIL_OP_DECR;
            default:
                assert(!"Invalid stencil op.");
                return D3D12_STENCIL_OP_KEEP;
            }
        }

        static D3D12_COMPARISON_FUNC ConvertComparisonFunc(ComparisonFunc value)
        {
            switch (value) {
            case ComparisonFunc::Never:
                return D3D12_COMPARISON_FUNC_NEVER;
            case ComparisonFunc::Less:
                return D3D12_COMPARISON_FUNC_LESS;
            case ComparisonFunc::Equal:
                return D3D12_COMPARISON_FUNC_EQUAL;
            case ComparisonFunc::LessOrEqual:
                return D3D12_COMPARISON_FUNC_LESS_EQUAL;
            case ComparisonFunc::Greater:
                return D3D12_COMPARISON_FUNC_GREATER;
            case ComparisonFunc::NotEqual:
                return D3D12_COMPARISON_FUNC_NOT_EQUAL;
            case ComparisonFunc::GreaterOrEqual:
                return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            case ComparisonFunc::Always:
                return D3D12_COMPARISON_FUNC_ALWAYS;
            default:
                assert(!"Invalid comparison func.");
                return D3D12_COMPARISON_FUNC_NEVER;
            }
        }

        static D3D12_DEPTH_STENCIL_DESC ConvertDepthStencilState(const DepthStencilState& inState)
        {
            D3D12_DEPTH_STENCIL_DESC outState{};
            outState.DepthEnable = inState.depthTestEnable ? TRUE : FALSE;
            outState.DepthWriteMask = inState.depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
            outState.DepthFunc = ConvertComparisonFunc(inState.depthFunc);
            outState.StencilEnable = inState.stencilEnable ? TRUE : FALSE;
            outState.StencilReadMask = (UINT8)inState.stencilReadMask;
            outState.StencilWriteMask = (UINT8)inState.stencilWriteMask;
            outState.FrontFace.StencilFailOp = ConvertStencilOp(inState.frontFaceStencil.failOp);
            outState.FrontFace.StencilDepthFailOp = ConvertStencilOp(inState.frontFaceStencil.depthFailOp);
            outState.FrontFace.StencilPassOp = ConvertStencilOp(inState.frontFaceStencil.passOp);
            outState.FrontFace.StencilFunc = ConvertComparisonFunc(inState.frontFaceStencil.stencilFunc);
            outState.BackFace.StencilFailOp = ConvertStencilOp(inState.backFaceStencil.failOp);
            outState.BackFace.StencilDepthFailOp = ConvertStencilOp(inState.backFaceStencil.depthFailOp);
            outState.BackFace.StencilPassOp = ConvertStencilOp(inState.backFaceStencil.passOp);
            outState.BackFace.StencilFunc = ConvertComparisonFunc(inState.backFaceStencil.stencilFunc);
            return outState;
        }

        static D3D_PRIMITIVE_TOPOLOGY ConvertPrimitiveType(PrimitiveType pt, uint32_t controlPoints)
        {
            switch (pt)
            {
            case PrimitiveType::PointList:
                return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            case PrimitiveType::LineList:
                return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            case PrimitiveType::LineStrip:
                return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case PrimitiveType::TriangleList:
                return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case PrimitiveType::TriangleStrip:
                return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            case PrimitiveType::TriangleFan:
                assert(!"TriangleFan is not supported.");
                return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            case PrimitiveType::TriangleListWithAdjacency:
                return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
            case PrimitiveType::TriangleStripWithAdjacency:
                return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
            case PrimitiveType::PatchList:
                if (controlPoints == 0 || controlPoints > 32) {
                    assert(!"Invalid number of control points for PatchList.");
                    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
                }
                return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoints - 1));
            default:
                return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            }
        }


        static D3D12_TEXTURE_ADDRESS_MODE ConvertAddressMode(SamplerAddressMode mode)
        {
            switch (mode) {
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
            switch (reductionType) {
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


        static D3D12_BLEND_DESC ConvertBlendState(const BlendState& inState)
        {
            D3D12_BLEND_DESC outState{};
            outState.AlphaToCoverageEnable = inState.alphaToCoverageEnable;
            outState.IndependentBlendEnable = true;

            for (uint32_t i = 0; i < c_MaxRenderTargets; i++) {
                const BlendState::RenderTarget& src = inState.targets[i];
                D3D12_RENDER_TARGET_BLEND_DESC& dst = outState.RenderTarget[i];

                dst.BlendEnable = src.blendEnable ? TRUE : FALSE;
                dst.SrcBlend = ConvertBlendValue(src.srcBlend);
                dst.DestBlend = ConvertBlendValue(src.destBlend);
                dst.BlendOp = ConvertBlendOp(src.blendOp);
                dst.SrcBlendAlpha = ConvertBlendValue(src.srcBlendAlpha);
                dst.DestBlendAlpha = ConvertBlendValue(src.destBlendAlpha);
                dst.BlendOpAlpha = ConvertBlendOp(src.blendOpAlpha);
                dst.RenderTargetWriteMask = (D3D12_COLOR_WRITE_ENABLE)src.colorWriteMask;
            }
            return outState;
        }

        static D3D12_RASTERIZER_DESC ConvertRasterizerState(const RasterState& inState)
        {
            D3D12_RASTERIZER_DESC outState{};
            switch (inState.fillMode) {
            case RasterFillMode::Solid:
                outState.FillMode = D3D12_FILL_MODE_SOLID;
                break;
            case RasterFillMode::Wireframe:
                outState.FillMode = D3D12_FILL_MODE_WIREFRAME;
                break;
            default:
                assert(!"Invalid fill mode.");
                break;
            }

            switch (inState.cullMode) {
            case RasterCullMode::Back:
                outState.CullMode = D3D12_CULL_MODE_BACK;
                break;
            case RasterCullMode::Front:
                outState.CullMode = D3D12_CULL_MODE_FRONT;
                break;
            case RasterCullMode::None:
                outState.CullMode = D3D12_CULL_MODE_NONE;
                break;
            default:
                assert(!"Invalid cull mode.");
                break;
            }

            outState.FrontCounterClockwise = inState.frontCounterClockwise ? TRUE : FALSE;
            outState.DepthBias = inState.depthBias;
            outState.DepthBiasClamp = inState.depthBiasClamp;
            outState.SlopeScaledDepthBias = inState.slopeScaledDepthBias;
            outState.DepthClipEnable = inState.depthClipEnable ? TRUE : FALSE;
            outState.MultisampleEnable = inState.multisampleEnable ? TRUE : FALSE;
            outState.AntialiasedLineEnable = inState.antialiasedLineEnable ? TRUE : FALSE;
            outState.ConservativeRaster = inState.conservativeRasterEnable ? 
                D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
            outState.ForcedSampleCount = inState.forcedSampleCount;
            
            return outState;
        }

        static DX12_ViewportState ConvertViewportState(const RasterState& rasterState, const FramebufferInfo& framebufferInfo, const ViewportState& vpState)
        {
            DX12_ViewportState ret;

            ret.numViewports = UINT(vpState.viewports.size());
            for (size_t rt = 0; rt < vpState.viewports.size(); rt++)
            {
                ret.viewports[rt].TopLeftX = vpState.viewports[rt].minX;
                ret.viewports[rt].TopLeftY = vpState.viewports[rt].minY;
                ret.viewports[rt].Width = vpState.viewports[rt].maxX - vpState.viewports[rt].minX;
                ret.viewports[rt].Height = vpState.viewports[rt].maxY - vpState.viewports[rt].minY;
                ret.viewports[rt].MinDepth = vpState.viewports[rt].minZ;
                ret.viewports[rt].MaxDepth = vpState.viewports[rt].maxZ;
            }

            ret.numScissorRects = UINT(vpState.scissorRects.size());
            for(size_t rt = 0; rt < vpState.scissorRects.size(); rt++)
            {
                if (rasterState.scissorEnable)
                {
                    ret.scissorRects[rt].left = (LONG)vpState.scissorRects[rt].minX;
                    ret.scissorRects[rt].top = (LONG)vpState.scissorRects[rt].minY;
                    ret.scissorRects[rt].right = (LONG)vpState.scissorRects[rt].maxX;
                    ret.scissorRects[rt].bottom = (LONG)vpState.scissorRects[rt].maxY;
                }
                else
                {
                    ret.scissorRects[rt].left = (LONG)vpState.viewports[rt].minX;
                    ret.scissorRects[rt].top = (LONG)vpState.viewports[rt].minY;
                    ret.scissorRects[rt].right = (LONG)vpState.viewports[rt].maxX;
                    ret.scissorRects[rt].bottom = (LONG)vpState.viewports[rt].maxY;

                    if (framebufferInfo.width > 0)
                    {
                        ret.scissorRects[rt].left = std::max(ret.scissorRects[rt].left, LONG(0));
                        ret.scissorRects[rt].top = std::max(ret.scissorRects[rt].top, LONG(0));
                        ret.scissorRects[rt].right = std::min(ret.scissorRects[rt].right, LONG(framebufferInfo.width));
                        ret.scissorRects[rt].bottom = std::min(ret.scissorRects[rt].bottom, LONG(framebufferInfo.height));
                    }
                }
            }

            return ret;
        }

        static D3D12_RESOURCE_DESC ConvertTextureDesc(const TextureDesc &desc)
        {
            const auto& formatMapping = GetDxgiFormatMapping(desc.format);
            const FormatInfo& formatInfo = GetFormatInfo(desc.format);

            D3D12_RESOURCE_DESC resourceDesc{};
            resourceDesc.Width = desc.width;
            resourceDesc.Height = desc.height;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = desc.mipLevels;
            resourceDesc.Format = desc.isTypeless ? formatMapping.resourceFormat : formatMapping.rtvFormat;
            resourceDesc.SampleDesc = {.Count = desc.sampleCount, .Quality = desc.sampleQuality};

            if(desc.isRenderTarget){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            }
            if(!desc.isShaderResource){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            }
            if(desc.isUAV){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            switch (desc.dimension)
            {
            case TextureDimension::Texture1D:
            case TextureDimension::Texture1DArray:
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
                resourceDesc.DepthOrArraySize = desc.arraySize;
                break;
            case TextureDimension::Texture2D:
            case TextureDimension::Texture2DArray:
            case TextureDimension::TextureCube:
            case TextureDimension::TextureCubeArray:
            case TextureDimension::Texture2DMS:
            case TextureDimension::Texture2DMSArray:
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                resourceDesc.DepthOrArraySize = desc.arraySize;
                break;
            case TextureDimension::Texture3D:
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
                resourceDesc.DepthOrArraySize = desc.depth;
                break;
            case TextureDimension::Unknown:
            default:
                assert("Invalid texture dimension.");
                break;
            }

            return resourceDesc;
        }

        static D3D12_CLEAR_VALUE ConvertClearValue(const TextureDesc &desc)
        {
            const auto& formatMapping = GetDxgiFormatMapping(desc.format);
            const FormatInfo& formatInfo = GetFormatInfo(desc.format);
            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = formatMapping.rtvFormat;
            if (formatInfo.hasDepth || formatInfo.hasStencil) {
                clearValue.DepthStencil.Depth = desc.clearValue.r;
                clearValue.DepthStencil.Stencil = UINT8(desc.clearValue.g);
            }
            else {
                clearValue.Color[0] = desc.clearValue.r;
                clearValue.Color[1] = desc.clearValue.g;
                clearValue.Color[2] = desc.clearValue.b;
                clearValue.Color[3] = desc.clearValue.a;
            }

            return clearValue;
        }

        static TextureDesc ConvertD3D12TextureDesc(
            const D3D12_RESOURCE_DESC &resourceDesc, 
            const std::string& name,
            bool isCubeMap)
        {
            TextureDesc desc{};
            desc.width = resourceDesc.Width;
            desc.height = resourceDesc.Height;
            desc.mipLevels = resourceDesc.MipLevels;
            desc.sampleCount = resourceDesc.SampleDesc.Count;
            desc.sampleQuality = resourceDesc.SampleDesc.Quality;
            desc.debugName = name;
            desc.initialState = ResourceStates::Common;
            
            std::span formatMapping = c_FormatMappings;
            for(auto it = formatMapping.begin(); it != formatMapping.end(); it++){
                auto format = resourceDesc.Format;
                if(it->resourceFormat == format || it->srvFormat == format || it->rtvFormat == format){
                    desc.format = it->abstractFormat;
                    break;
                }
            }
            
            if(HasFlags(resourceDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)){
                desc.isRenderTarget = true;
            }
            if(HasFlags(resourceDesc.Flags, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)){
                desc.isShaderResource = false;
            }
            if(HasFlags(resourceDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)){
                desc.isUAV = true;
            }

            switch (resourceDesc.Dimension)
            {
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D:{
                if(resourceDesc.DepthOrArraySize > 1){
                    desc.arraySize = resourceDesc.DepthOrArraySize;
                    desc.dimension = TextureDimension::Texture1DArray;
                }
                else{
                    desc.dimension = TextureDimension::Texture1D;
                }
                break;
            }
            case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
                if(resourceDesc.DepthOrArraySize > 6 && isCubeMap){
                    desc.arraySize = resourceDesc.DepthOrArraySize;
                    desc.dimension = desc.arraySize == 6 ? TextureDimension::TextureCube : TextureDimension::TextureCubeArray;
                }
                else if(resourceDesc.DepthOrArraySize > 1){
                    desc.arraySize = resourceDesc.DepthOrArraySize;
                    desc.dimension = desc.sampleCount > 1 ? TextureDimension::Texture2DMSArray : TextureDimension::Texture2DArray;
                }
                else{
                    desc.dimension = desc.sampleCount > 1 ? TextureDimension::Texture2DMS : TextureDimension::Texture2D;
                }
                break;
            }
            case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
                desc.dimension = TextureDimension::Texture3D;
                desc.depth = resourceDesc.DepthOrArraySize;
                break;
            case D3D12_RESOURCE_DIMENSION_UNKNOWN:
            default:
                assert("Invalid texture dimension.");
                break;
            }

            return desc;
        }

        static void WaitForFence(ID3D12Fence* fence, uint64_t fenceValue, HANDLE event)
        {
            // 进行等待
            if(fence->GetCompletedValue() < fenceValue){
                ResetEvent(event);
                fence->SetEventOnCompletion(fenceValue, event);
                WaitForSingleObject(event, INFINITE);
            }
        }

        // 计算子资源所对应的索引
        inline uint32_t CalculateSubresource(
            uint32_t mipSlice, uint32_t arraySlice, uint32_t planeSlice, 
            uint32_t mipLevels, uint32_t arraySize)
        {
            return mipSlice + arraySlice * mipLevels + planeSlice * arraySize * mipLevels;
        }



    }
}


#endif
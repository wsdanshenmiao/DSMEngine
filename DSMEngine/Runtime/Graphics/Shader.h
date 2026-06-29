#pragma once
#ifndef __SHADER_H__
#define __SHADER_H__

#include "GraphicsCommon.h"

namespace DSM {
    enum class ShaderType : uint16_t
    {
        None            = 0x0000,

        Compute         = 0x0020,

        Vertex          = 0x0001,
        Hull            = 0x0002,
        Domain          = 0x0004,
        Geometry        = 0x0008,
        Pixel           = 0x0010,
        Amplification   = 0x0040,
        Mesh            = 0x0080,
        AllGraphics     = 0x00DF,

        RayGeneration   = 0x0100,
        AnyHit          = 0x0200,
        ClosestHit      = 0x0400,
        Miss            = 0x0800,
        Intersection    = 0x1000,
        Callable        = 0x2000,
        AllRayTracing   = 0x3F00,

        All             = 0x3FFF,
    };
    ENABLE_ENUM_BIT_OPERATOR(ShaderType)

    struct ShaderDesc
    {
        ShaderType shaderType = ShaderType::None;
        std::string debugName;
        std::string entryName = "main";

        constexpr ShaderDesc& SetShaderType(ShaderType value) { shaderType = value; return *this; }

        ShaderDesc& SetDebugName(const std::string& value) { debugName = value; return *this; }
        ShaderDesc& SetEntryName(const std::string& value) { entryName = value; return *this; }

        bool operator==(const ShaderDesc& other) const = default;
    };

    struct IShader : public IResource
    {
        [[nodiscard]] virtual const ShaderDesc& GetDesc() const = 0;
        virtual void GetBytecode(const void** ppBytecode, size_t* pSize) const = 0;
    };
    using ShaderHandle = RefPtr<IShader>;

    class IShaderLibrary : public IResource
    {
    public:
        virtual void GetBytecode(const void** ppBytecode, size_t* pSize) const = 0;
        virtual ShaderHandle GetShader(const char* entryName, ShaderType shaderType) = 0;
    };
    using ShaderLibraryHandle = RefPtr<IShaderLibrary>;

} // namespace DSM 

#endif
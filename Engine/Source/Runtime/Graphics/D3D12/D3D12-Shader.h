#pragma once
#ifndef __D3D12_SHADER_H__
#define __D3D12_SHADER_H__


#include "Runtime/Graphics/Shader.h"

namespace DSM::D3D12{
    class Shader : public IShader
    {
    public:
        Shader(ShaderDesc desc) : m_Desc(desc) {}

        const ShaderDesc& GetDesc() const override { return m_Desc; }
        void GetBytecode(const void** ppBytecode, size_t* pSize) const override
        {
            assert(ppBytecode != nullptr && pSize != nullptr);
            *ppBytecode = bytecode.data();
            *pSize = bytecode.size();
        }

    public:
        std::vector<uint8_t> bytecode;

    private:
        ShaderDesc m_Desc;
    };

    class ShaderLibraryEntry : public IShader
    {
    public:
        ShaderLibraryEntry(IShaderLibrary* pLibrary, const char* entryName, ShaderType shaderType)
            : library(pLibrary)
        {
            m_Desc.shaderType = shaderType;
            m_Desc.entryName = entryName;
        }

        const ShaderDesc& GetDesc() const override { return m_Desc; }
        void GetBytecode(const void** ppBytecode, size_t* pSize) const override
        {
            library->GetBytecode(ppBytecode, pSize);
        }

    public:
        ShaderLibraryHandle library;

    private:
        ShaderDesc m_Desc;
    };

    class ShaderLibrary : public IShaderLibrary
    {
    public:
        void GetBytecode(const void** ppBytecode, size_t* pSize) const override
        {
            assert(ppBytecode != nullptr && pSize != nullptr);
            *ppBytecode = bytecode.data();
            *pSize = bytecode.size();
        }
        ShaderHandle GetShader(const char* entryName, ShaderType shaderType) override
        {
            return ShaderHandle{ new ShaderLibraryEntry(this, entryName, shaderType) };
        }

    public:
        std::vector<char> bytecode;
    };


}


#endif
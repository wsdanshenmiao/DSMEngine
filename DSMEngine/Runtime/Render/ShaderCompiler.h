#pragma once
#ifndef __SHADERCOMPILER_H__
#define __SHADERCOMPILER_H__

#include "Runtime/Utils/Utils.h"
#include "Runtime/Graphics/Shader.h"

#include <dxcapi.h>
#include <vector>
#include <map>
#include <string>
#include <unordered_map>
#include <d3d12.h>

namespace DSM {
    class ShaderDefines
    {
    public:
        ShaderDefines() = default;
        ShaderDefines(std::initializer_list<std::pair<std::string, std::string>> initList)
        {
			for (const auto& pair : initList) {
				AddDefine(pair.first, pair.second);
			}
        }
        ShaderDefines(const ShaderDefines& other) = default;
		ShaderDefines& operator=(const ShaderDefines& other) = default;
        ShaderDefines(ShaderDefines&&) = default;
        ShaderDefines& operator=(ShaderDefines&&) = default;

        void AddDefine(const std::string& name, const std::string& value)
        {
            m_Defines[Utility::UTF8ToWString(name)] = Utility::UTF8ToWString(value);
        }
        void RemoveDefine(const std::string& name)
        {
            std::wstring wname = Utility::UTF8ToWString(name);
            if (m_Defines.contains(wname)) {
                m_Defines.erase(wname);
            }
        }
        std::vector<DxcDefine> Finish() const
        {
            std::vector<DxcDefine> result{};
            result.reserve(m_Defines.size() + 1);
            for (const auto& define : m_Defines) {
                result.emplace_back(DxcDefine{.Name = define.first.c_str(), .Value = define.second.c_str()});
            }
            return result;
        }

        const auto& GetDefines() const noexcept { return m_Defines; }

        bool operator==(const ShaderDefines& other) const = default;

    private:
        std::map<std::wstring, std::wstring> m_Defines;
    };

    enum class ShaderMode
    {
        SM_6_0,
        SM_6_1,
        SM_6_2,
        SM_6_3,
        SM_6_4,
        SM_6_5,
        SM_6_6,
        SM_6_7,
        SM_6_8
    };

    struct ShaderCompileDesc
    {
        ShaderType type;
        ShaderMode mode;
        std::string fileName;
        std::string enterPoint;
        ShaderDefines defines;

        ShaderCompileDesc& SetType(ShaderType _type) { type = _type; return *this; }
        ShaderCompileDesc& SetMode(ShaderMode _mode) { mode = _mode; return *this; }
        ShaderCompileDesc& SetFilename(const std::string& name) { fileName = name; return *this; }
        ShaderCompileDesc& SetEnterPoint(const std::string& _enterPoint) { enterPoint = _enterPoint; return *this; }
        ShaderCompileDesc& SetDefine(ShaderDefines _defines) { defines = std::move(_defines); return *this; }
        ShaderCompileDesc& AddDefine(const std::string& name, const std::string& val) { defines.AddDefine(name, val); return *this; }
    
        bool operator==(const ShaderCompileDesc& other) const = default;
    };
    
    class ShaderByteCode
    {
        friend class ShaderCompiler;
    public:
        ShaderByteCode(const ShaderCompileDesc& shaderDesc);
        ~ShaderByteCode() = default;

        const void* GetByteCode() const noexcept { return m_ByteCode.data(); }
        std::uint64_t GetByteCodeSize() const noexcept { return m_ByteCode.size(); }
        const ShaderCompileDesc& GetDesc() const noexcept { return m_Desc; }

        bool IsValid() const noexcept { return !m_ByteCode.empty(); }

        bool operator==(const ShaderByteCode& other) const = default;

    private:
        const ShaderCompileDesc m_Desc;
        std::vector<std::uint8_t> m_ByteCode{};
    };

}

template <>
struct std::hash<DSM::ShaderDefines>
{
    std::size_t operator()(const DSM::ShaderDefines& d) const
    {
        std::size_t h = 0;
        for (const auto& define : d.GetDefines()) {
            h = DSM::Utility::HashCombine(h, define.first);
            h = DSM::Utility::HashCombine(h, define.second);
        }
        return h;
    }
};

template <>
struct std::hash<DSM::ShaderCompileDesc>
{
    std::size_t operator()(const DSM::ShaderCompileDesc& desc) const
    {
        using namespace DSM::Utility;
        std::size_t h = 0;
        h = HashCombine(h, static_cast<std::underlying_type_t<DSM::ShaderType>>(desc.type));
        h = HashCombine(h, static_cast<std::underlying_type_t<DSM::ShaderMode>>(desc.mode));
        h = HashCombine(h, desc.fileName);
        h = HashCombine(h, desc.enterPoint);
        h = HashCombine(h, desc.defines);
        return h;
    }
};


#endif

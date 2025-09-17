#pragma once
#ifndef __SHADERCOMPILER_H__
#define __SHADERCOMPILER_H__

#include <Windows.h>
#include <dxcapi.h>
#include <vector>
#include <map>
#include <string>
#include <unordered_map>
#include <d3d12.h>
#include "Runtime/Utils/Utils.h"
#include "Runtime/Graphics/Shader.h"

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
        ShaderCompileDesc& AddDefine(const std::string& name, const std::string& val) { defines.AddDefine(name, val); return *this; }
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

    private:
        const ShaderCompileDesc m_Desc;
        std::vector<std::uint8_t> m_ByteCode{};
    };

}


#endif

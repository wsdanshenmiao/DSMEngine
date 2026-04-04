#include "Shader.h"


namespace DSM {
    ShaderHandle Shader::GetPass(const std::string &entryPoint, ShaderType type)
    {
        auto pass = ShaderPass{}.SetEntryPoint(entryPoint).SetType(type);
        ShaderDefines defines{};
        for(const auto& [keyword, value] : sm_GlobalKeywords) {
            pass.AddDefine(keyword, value);
            defines.AddDefine(keyword, value);
        }
        for(const auto& [keyword, value] : m_LocalKeywords) {
            pass.AddDefine(keyword, value);
            defines.AddDefine(keyword, value);
        }
        if(!sm_CompiledShaders.contains(pass)){
            ShaderByteCode bytecode{ShaderCompileDesc{}
                .SetFilename(m_FilePath)
                .SetEnterPoint(entryPoint)
                .SetType(type)
                .SetMode(ShaderMode::SM_6_6)
                .SetDefine(defines)};
            auto device = DSMEngine::sm_GlobalContext.renderer->GetDevice();
            sm_CompiledShaders[pass] = device->CreateShader(ShaderDesc{}
                .SetShaderType(type)
                .SetDebugName(m_FilePath + ":" + entryPoint)
                .SetEntryName(entryPoint), 
                bytecode.GetByteCode(), bytecode.GetByteCodeSize());
        }
        return sm_CompiledShaders[pass];
    }

    void Shader::EnableKeyword(const std::string &keyword, const std::string &value)
    {
        m_LocalKeywords[keyword] = value;
    }

    void Shader::DisableKeyword(const std::string &keyword)
    {
        m_LocalKeywords.erase(keyword);
    }

    void Shader::ClearCache() noexcept
    {
        sm_Shaders.clear();
        sm_CompiledShaders.clear();
        sm_GlobalKeywords.clear();
    }

    std::shared_ptr<Shader> DSM::Shader::Find(const std::string &filePath)
    {
        if(auto it = sm_Shaders.find(filePath); it != sm_Shaders.end()) {
            it->second->m_LocalKeywords.clear();
            return it->second;
        } 
        else {
            auto shader = std::shared_ptr<Shader>(new Shader(filePath));
            sm_Shaders[filePath] = shader;
            return shader;
        }
    }
    
    void Shader::EnableGlobalKeyword(const std::string &keyword, const std::string &value)
    {
        sm_GlobalKeywords[keyword] = value;
    }
    
    void Shader::DisableGlobalKeyword(const std::string &keyword)
    {
        sm_GlobalKeywords.erase(keyword);
    }
}
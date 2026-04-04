#pragma once
#ifndef __RENDER_SHADER_H__
#define __RENDER_SHADER_H__

#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Graphics/Shader.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"

namespace DSM {
    class Shader
    {
        struct ShaderPass
        {
            std::string entryPoint{};
            ShaderType type = ShaderType::None;
            std::map<std::string, std::string> defines{};

            struct Hasher
            {
                size_t operator()(const ShaderPass& pass) const noexcept
                {
                    size_t hash = std::hash<std::string>{}(pass.entryPoint) ^ std::hash<int>{}(static_cast<int>(pass.type));
                    for(const auto& [define, value] : pass.defines){
                        hash ^= std::hash<std::string>{}(define) ^ std::hash<std::string>{}(value);
                    }
                    return hash;
                }
            };

            [[nodiscard]] bool operator==(const ShaderPass& other) const noexcept = default;

            constexpr ShaderPass& SetType(ShaderType value) { type = value; return *this; }
            constexpr ShaderPass& SetEntryPoint(const std::string& value) { entryPoint = value; return *this; }
            constexpr ShaderPass& AddDefine(const std::string& name, const std::string& value) { defines[name] = value; return *this; }
        };

        Shader(std::string filePath) : m_FilePath(std::move(filePath)) {}

    public:
        ShaderHandle GetPass(const std::string& entryPoint, ShaderType type);
        void EnableKeyword(const std::string& keyword, const std::string& value);
        void DisableKeyword(const std::string& keyword);

        static void ClearCache() noexcept;
        static std::shared_ptr<Shader> Find(const std::string& filePath);
        static void EnableGlobalKeyword(const std::string& keyword, const std::string& value);
        static void DisableGlobalKeyword(const std::string& keyword);

    private:
        inline static std::unordered_map<std::string, std::shared_ptr<Shader>> sm_Shaders{};
        // 已经编译过的 shader pass，避免重复编译
        inline static std::unordered_map<ShaderPass, ShaderHandle, ShaderPass::Hasher> sm_CompiledShaders{};
        // 全局启用的 shader keyword，所有 shader pass 都会使用这些 keyword 进行编译
        inline static std::unordered_map<std::string, std::string> sm_GlobalKeywords{};

        // Shader 的文件路径
        std::string m_FilePath;
        std::unordered_map<std::string, std::string> m_LocalKeywords{};
    };

} // namespace DSM

#endif // !__RENDER_SHADER_H__
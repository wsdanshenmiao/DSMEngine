#pragma once
#ifndef __MATERIAL_H__
#define __MATERIAL_H__

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "Runtime/Render/Shader.h"
#include "Shaders/ForwardShader/ResourceData.h"


namespace DSM {
    class Material
    {
    public:
        Material(std::shared_ptr<Shader> shader)
            : m_Shader(std::move(shader)) {}

        void EnableKeyword(const std::string& keyword, const std::string& value)
        { 
            if (m_Shader != nullptr) 
                m_Shader->EnableKeyword(keyword, value);
        }
        void DisableKeyword(const std::string& keyword) { if (m_Shader) m_Shader->DisableKeyword(keyword); }
        void FindPass(const std::string& entryPoint, ShaderType type)
        {
            if (m_Shader) 
                m_Shader->GetPass(entryPoint, type);
        }

        const Math::Vector4& GetBaseColor() const noexcept { return m_BaseColor; }
        void SetBaseColor(const Math::Vector4& color) noexcept { m_BaseColor = color; }

        const Math::Vector4& GetEmissiveColor() const noexcept { return m_EmissiveColor; }
        void SetEmissiveColor(const Math::Vector4& color) noexcept { m_EmissiveColor = color; }

        float GetNormalTexScale() const noexcept { return m_NormalTexScale; }
        void SetNormalTexScale(float scale) noexcept { m_NormalTexScale = scale; }

        float GetMetallicFactor() const noexcept { return m_MetallicFactor; }
        void SetMetallicFactor(float factor) noexcept { m_MetallicFactor = factor; }

        float GetRoughnessFactor() const noexcept { return m_RoughnessFactor; }
        void SetRoughnessFactor(float factor) noexcept { m_RoughnessFactor = factor; }

        TextureHandle GetTexture(ShaderResource::MaterialTex index) const noexcept { return m_Textures[index]; }
        void SetTexture(ShaderResource::MaterialTex index, const TextureHandle& texture) noexcept { m_Textures[index] = texture; }

        inline auto GetTextures() const noexcept { return m_Textures; }
        inline void SetTextures(std::array<TextureHandle, ShaderResource::kNumTextures> tex) noexcept { m_Textures = std::move(tex); }

        inline bool IsBothSide() const noexcept { return m_BothSide; }
        inline void SetBothSide(bool bothSide) noexcept { m_BothSide = bothSide; }

        inline bool IsTransparent() const noexcept { return m_Transparent; }
        inline void SetTransparent(bool transparent) noexcept { m_Transparent = transparent; }

    private:
        std::shared_ptr<Shader> m_Shader{};
        
        std::array<TextureHandle, ShaderResource::kNumTextures> m_Textures{};
        Math::Vector4 m_BaseColor{1.0f, 1.0f, 1.0f, 1.0f};
        Math::Vector4 m_EmissiveColor{0.0f, 0.0f, 0.0f, 0.0f};
        float m_NormalTexScale{1.0f};
        float m_MetallicFactor{0.0f};
        float m_RoughnessFactor{1.0f};

        bool m_BothSide{false};
        bool m_Transparent{false};
    };
} // namespace DSM


#endif // !__MATERIAL_H__
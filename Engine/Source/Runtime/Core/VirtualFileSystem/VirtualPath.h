#pragma once
#ifndef __DSM_VIRTUAL_PATH_H__
#define __DSM_VIRTUAL_PATH_H__

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace DSM
{
    enum class PathErrorCode
    {
        InvalidFormat,
        InvalidMount,
        Traversal,
        NotFound,
        ReadOnly,
        AlreadyMounted,
        NotMounted,
        OutsideMount,
    };

    struct PathError
    {
        PathErrorCode code{};
        std::string message{};
    };

    struct VirtualPath
    {
        std::string mount{};
        std::filesystem::path relativePath{};

        inline static constexpr std::string_view s_EngineMount = "/Engine/";
        inline static constexpr std::string_view s_ProjectMount = "/Game/";
        inline static constexpr std::string_view s_GameShaderMount = "/GameShaders/";
        inline static constexpr std::string_view s_EngineShaderMount = "/EngineShaders/";

        static std::expected<VirtualPath, PathError> Parse(std::string_view value);

        [[nodiscard]] std::string ToString() const;
        [[nodiscard]] bool IsEnginePath() const noexcept;
        [[nodiscard]] bool IsProjectPath() const noexcept;
        [[nodiscard]] bool IsShaderPath() const noexcept;
    };
}

#endif

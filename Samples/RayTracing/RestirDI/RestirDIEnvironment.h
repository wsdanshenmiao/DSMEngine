#pragma once

#include "RestirDIAliasTable.h"
#include "RestirDISettings.h"

#include <filesystem>
#include <string>
#include <vector>

namespace DSM::RestirDI {

    struct EnvironmentData
    {
        EnvironmentSource source = EnvironmentSource::DaylightCube;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<GpuFloat4> pixels{};
        AliasTable aliasTable{};
        std::filesystem::path sourcePath{};
    };

    [[nodiscard]] bool LoadDaylightEnvironment(
        const std::filesystem::path& assetsDirectory,
        EnvironmentData& output,
        std::string& error);

    [[nodiscard]] bool LoadRadianceEnvironment(
        const std::filesystem::path& filename,
        EnvironmentData& output,
        std::string& error);

}

#pragma once

#include <cstdint>
#include <filesystem>

namespace DSM::RestirDI {

    struct ValidationOptions
    {
        std::filesystem::path outputDirectory{};
        uint32_t editorFrameCount = 120;
    };

    int RunRenderValidation(const ValidationOptions& options);
    int RunEditorValidation(const ValidationOptions& options);

}

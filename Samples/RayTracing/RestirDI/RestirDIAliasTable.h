#pragma once

#include "RestirDIShared.h"

#include <span>
#include <vector>

namespace DSM::RestirDI {

    struct AliasTable
    {
        std::vector<GpuAliasEntry> entries{};
        float totalWeight = 0.0f;

        [[nodiscard]] bool Empty() const noexcept { return entries.empty(); }
    };

    [[nodiscard]] AliasTable BuildAliasTable(std::span<const float> weights);

}

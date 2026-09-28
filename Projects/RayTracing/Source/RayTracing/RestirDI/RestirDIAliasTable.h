#pragma once

#include "RestirDIShared.h"

#include <span>
#include <vector>

namespace DSM::RestirDI {

    // CPU 端的离散重要性采样表，不是 ReSTIR Reservoir。
    // Alias Table 负责按给定权重快速生成候选；Reservoir 再从候选流中保留一个样本。
    // entries 会原样上传到 GPU，totalWeight 用于计算解析灯、自发光和环境三个域的混合概率。
    struct AliasTable
    {
        std::vector<GpuAliasEntry> entries{};
        float totalWeight = 0.0f;

        [[nodiscard]] bool Empty() const noexcept { return entries.empty(); }
    };

    // Walker Alias Method：O(N) 预处理任意非负离散权重，换取 GPU 端 O(1) 抽样。
    [[nodiscard]] AliasTable BuildAliasTable(std::span<const float> weights);

}

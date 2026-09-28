#include "RestirDIAliasTable.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace DSM::RestirDI {

    AliasTable BuildAliasTable(std::span<const float> weights)
    {
        // Walker Alias Table 把任意离散权重 O(N) 预处理成 O(1) 抽样：
        // pmf 是真实候选 PDF，probability/alias 只负责完成一次等概率列选择。
        // 这对应论文第 5 节对灯光、发光三角形和环境 texel 的功率采样。
        AliasTable result{};
        if (weights.empty()) {
            return result;
        }

        result.entries.resize(weights.size());
        std::vector<float> sanitized(weights.size());
        // 将权重限制在 [0, +inf) 范围，避免负数或 NaN 破坏 Alias Table 构建。
        std::ranges::transform(weights, sanitized.begin(), [](float weight) {
            return std::isfinite(weight) ? std::max(weight, 0.0f) : 0.0f;
        });
        result.totalWeight = std::accumulate(sanitized.begin(), sanitized.end(), 0.0f);

        if (result.totalWeight <= 0.0f) {
            // 全零分布没有可定义的 proposal；退化为均匀分布，保证 q > 0，
            // 避免 RIS 的 pHat / q 出现除零，同时保留候选域可恢复运行。
            std::fill(sanitized.begin(), sanitized.end(), 1.0f);
            result.totalWeight = static_cast<float>(sanitized.size());
        }

        const float entryCount = static_cast<float>(sanitized.size());
        std::vector<float> scaled(sanitized.size());
        // 自身的概率列不足以填满一列
        std::vector<uint32_t> smallEntries{};
        // 自身概率至少能填满一列，可将剩余容量分配给小概率列
        std::vector<uint32_t> largeEntries{};
        smallEntries.reserve(sanitized.size());
        largeEntries.reserve(sanitized.size());

        for (uint32_t index = 0; index < sanitized.size(); ++index) {
            // pmf 是归一化后的真实离散概率，后续计算 RIS 权重 pHat/q 时需要它。
            result.entries[index].pmf = sanitized[index] / result.totalWeight;
            // scaled 用于把任意 PMF 拆成 N 个等概率入口，每列的容量均为 1。
            scaled[index] = result.entries[index].pmf * entryCount;
            (scaled[index] < 1.0f ? smallEntries : largeEntries).push_back(index);
        }

        // 用大概率列的剩余容量填补小概率列。最终抽样只需随机选择一列，
        // 再通过 probability 判断返回本列还是 alias，因此查询复杂度为 O(1)。
        while (!smallEntries.empty() && !largeEntries.empty()) {
            const uint32_t smallIndex = smallEntries.back();
            const uint32_t largeIndex = largeEntries.back();
            smallEntries.pop_back();
            largeEntries.pop_back();

            result.entries[smallIndex].probability = std::clamp(scaled[smallIndex], 0.0f, 1.0f);
            result.entries[smallIndex].alias = largeIndex;
            scaled[largeIndex] = (scaled[largeIndex] + scaled[smallIndex]) - 1.0f;
            (scaled[largeIndex] < 1.0f ? smallEntries : largeEntries).push_back(largeIndex);
        }

        for (uint32_t index : largeEntries) {
            result.entries[index].probability = 1.0f;
            result.entries[index].alias = index;
        }
        for (uint32_t index : smallEntries) {
            result.entries[index].probability = 1.0f;
            result.entries[index].alias = index;
        }
        return result;
    }

}

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
        std::transform(weights.begin(), weights.end(), sanitized.begin(), [](float weight) {
            return std::isfinite(weight) ? std::max(weight, 0.0f) : 0.0f;
        });
        result.totalWeight = std::accumulate(sanitized.begin(), sanitized.end(), 0.0f);

        if (!(result.totalWeight > 0.0f)) {
            // 全零分布没有可定义的 proposal；退化为均匀分布，保证 q>0，
            // 避免 RIS 的 pHat/q 出现除零，同时保留候选域可恢复运行。
            std::fill(sanitized.begin(), sanitized.end(), 1.0f);
            result.totalWeight = static_cast<float>(sanitized.size());
        }

        const float entryCount = static_cast<float>(sanitized.size());
        std::vector<float> scaled(sanitized.size());
        std::vector<uint32_t> smallEntries{};
        std::vector<uint32_t> largeEntries{};
        smallEntries.reserve(sanitized.size());
        largeEntries.reserve(sanitized.size());

        for (uint32_t index = 0; index < sanitized.size(); ++index) {
            result.entries[index].pmf = sanitized[index] / result.totalWeight;
            scaled[index] = result.entries[index].pmf * entryCount;
            (scaled[index] < 1.0f ? smallEntries : largeEntries).push_back(index);
        }

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

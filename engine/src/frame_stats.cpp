#include <phantasm/frame_stats.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace phantasm {

namespace {

double nearest_rank(const std::vector<double>& sorted, double percentile) {
    const auto rank = static_cast<std::size_t>(
        std::ceil(percentile / 100.0 * static_cast<double>(sorted.size())));
    return sorted[std::clamp<std::size_t>(rank, 1, sorted.size()) - 1];
}

} // namespace

FrameTimeSummary summarize(std::span<const double> samples) {
    if (samples.empty()) {
        return {};
    }
    std::vector<double> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    return FrameTimeSummary{
        .count = sorted.size(),
        .mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) /
                static_cast<double>(sorted.size()),
        .p50 = nearest_rank(sorted, 50.0),
        .p99 = nearest_rank(sorted, 99.0),
        .max = sorted.back(),
    };
}

} // namespace phantasm

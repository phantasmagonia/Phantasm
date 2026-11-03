#pragma once

#include <cstddef>
#include <span>

namespace phantasm {

struct FrameTimeSummary {
    std::size_t count = 0;
    double mean = 0.0;
    double p50 = 0.0;
    double p99 = 0.0;
    double max = 0.0;
};

// Percentiles use the nearest-rank method.
[[nodiscard]] FrameTimeSummary summarize(std::span<const double> samples);

} // namespace phantasm

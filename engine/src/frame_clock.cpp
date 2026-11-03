#include <phantasm/frame_clock.hpp>

#include <algorithm>

namespace phantasm {

namespace {
constexpr double kSecondsPerNs = 1e-9;
}

FrameClock::FrameClock(std::uint64_t start_ns) noexcept : last_ns_{start_ns} {}

double FrameClock::tick(std::uint64_t now_ns) noexcept {
    // Guards against unsigned wraparound.
    if (now_ns <= last_ns_) {
        return 0.0;
    }
    const std::uint64_t elapsed_ns = now_ns - last_ns_;
    last_ns_ = now_ns;
    return std::min(static_cast<double>(elapsed_ns) * kSecondsPerNs, kMaxDeltaSeconds);
}

} // namespace phantasm

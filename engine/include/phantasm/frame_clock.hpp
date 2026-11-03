#pragma once

#include <cstdint>

namespace phantasm {

// Turns raw timestamps (nanoseconds) into a per-frame "delta time" in seconds.
// Pure logic, no SDL: the caller supplies timestamps, which makes it testable.
class FrameClock {
public:
    // Largest delta ever returned. Prevents a long stall (debugger, window drag)
    // from producing one giant step that teleports objects through walls.
    static constexpr double kMaxDeltaSeconds = 0.25;

    explicit FrameClock(std::uint64_t start_ns) noexcept;

    // Seconds since the previous tick, clamped to [0, kMaxDeltaSeconds].
    double tick(std::uint64_t now_ns) noexcept;

private:
    std::uint64_t last_ns_;
};

} // namespace phantasm

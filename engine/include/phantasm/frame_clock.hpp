#pragma once

#include <cstdint>

namespace phantasm {

class FrameClock {
public:
    static constexpr double kMaxDeltaSeconds = 0.25;

    explicit FrameClock(std::uint64_t start_ns) noexcept;

    double tick(std::uint64_t now_ns) noexcept;

private:
    std::uint64_t last_ns_;
};

} // namespace phantasm

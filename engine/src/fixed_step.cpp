#include <phantasm/fixed_step.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace phantasm {

namespace {
// Absorbs rounding so e.g. 3 * step / step counts as 3 steps, not 2.999...
constexpr double kStepTolerance = 1e-9;
} // namespace

FixedStep::FixedStep(double step_seconds, int max_steps_per_frame)
    : step_{step_seconds}, max_steps_{max_steps_per_frame} {
    if (!(step_seconds > 0.0) || max_steps_per_frame < 1) {
        throw std::invalid_argument("FixedStep needs step_seconds > 0 and max_steps_per_frame >= 1");
    }
}

int FixedStep::advance(double frame_seconds) noexcept {
    if (!(frame_seconds > 0.0)) {
        return 0;
    }
    accumulator_ += frame_seconds;
    const auto due = static_cast<int>(std::floor(accumulator_ / step_ + kStepTolerance));
    if (due > max_steps_) {
        accumulator_ = 0.0;
        return max_steps_;
    }
    accumulator_ = std::max(0.0, accumulator_ - static_cast<double>(due) * step_);
    return due;
}

} // namespace phantasm

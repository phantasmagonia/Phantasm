#pragma once

namespace phantasm {

// Turns variable frame times into a whole number of fixed simulation steps.
// Usage: for (int i = stepper.advance(dt); i > 0; --i) { world.step(stepper.step()); }
class FixedStep {
public:
    // Throws std::invalid_argument if step_seconds <= 0 or max_steps_per_frame < 1.
    explicit FixedStep(double step_seconds, int max_steps_per_frame = 8);

    // Returns how many steps to run this frame. Time beyond max_steps_per_frame is
    // dropped, so one slow frame cannot snowball into ever-slower frames.
    int advance(double frame_seconds) noexcept;

    [[nodiscard]] double step() const noexcept { return step_; }
    // Leftover fraction of a step in [0, 1): use to interpolate rendered positions.
    [[nodiscard]] double alpha() const noexcept { return accumulator_ / step_; }

private:
    double step_;
    int max_steps_;
    double accumulator_ = 0.0;
};

} // namespace phantasm

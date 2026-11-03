#pragma once

namespace phantasm {

struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;
};

// Axis-aligned rectangle; (x, y) is the top-left corner.
struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float w = 0.0F;
    float h = 0.0F;
};

// Maps a rect in pixels (origin top-left, y down) to clip space
// (origin center, y up, [-1, 1]). The result's h is negative.
[[nodiscard]] Rect to_clip_rect(Rect pixels, Vec2 viewport) noexcept;

} // namespace phantasm

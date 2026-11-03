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

} // namespace phantasm

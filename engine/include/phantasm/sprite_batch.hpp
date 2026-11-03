#pragma once

#include <phantasm/color.hpp>
#include <phantasm/geometry.hpp>

#include <cstdint>
#include <vector>

namespace phantasm {

class Texture;

// Per-sprite instance data, read by the vertex shader as vertex attributes.
struct SpriteInstance {
    Rect rect;  // world space
    Rect uv;
    Color tint;
};
static_assert(sizeof(SpriteInstance) == 48);

struct SpriteDraw {
    const Texture* texture = nullptr;
    int layer = 0;
    float depth = 0.0F;
    SpriteInstance instance{};
};

// A contiguous range of sorted sprites sharing one texture: one draw call.
struct DrawRun {
    const Texture* texture = nullptr;
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

// Sorts by (layer, depth, texture) and returns the draw runs.
std::vector<DrawRun> sort_and_group(std::vector<SpriteDraw>& sprites);

} // namespace phantasm

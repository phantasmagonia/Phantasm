#pragma once

#include <phantasm/color.hpp>
#include <phantasm/geometry.hpp>

namespace phantasm {

struct Sprite {
    Vec2 position{}; // world position of the pivot
    Vec2 size{};
    // Normalized anchor inside the sprite: (0, 0) top-left, (0.5, 1) bottom-center.
    Vec2 pivot{};
    Rect uv{0.0F, 0.0F, 1.0F, 1.0F};
    Color tint{1.0F, 1.0F, 1.0F, 1.0F};
    int layer = 0;
    // Within a layer, lower depth draws first. Isometric: use world y.
    float depth = 0.0F;
};

[[nodiscard]] Rect sprite_world_rect(const Sprite& sprite) noexcept;

} // namespace phantasm

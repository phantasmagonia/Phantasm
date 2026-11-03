#pragma once

#include <phantasm/geometry.hpp>

namespace phantasm::iso {

// Continuous grid position; tile (i, j) covers [i, i+1) x [j, j+1).
struct GridPos {
    float x = 0.0F;
    float y = 0.0F;
    int level = 0; // floor level, 0 = ground
};

struct TileCoord {
    int x = 0;
    int y = 0;
    int level = 0;
};

// 2:1 diamond projection between grid space and world space.
// Grid +x runs down-right on screen, grid +y runs down-left.
struct IsoGrid {
    float tile_width = 64.0F;
    float tile_height = 32.0F;
    float level_height = 96.0F; // world units one floor level is raised by

    [[nodiscard]] Vec2 grid_to_world(GridPos grid) const noexcept;
    [[nodiscard]] GridPos world_to_grid(Vec2 world, int level) const noexcept;
    [[nodiscard]] TileCoord pick_tile(Vec2 world, int level) const noexcept;
};

// Sprite depth within a floor level (use the level as the sprite layer).
[[nodiscard]] float draw_depth(GridPos grid) noexcept;

} // namespace phantasm::iso

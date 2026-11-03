#include <phantasm/iso/iso_grid.hpp>

#include <cmath>

namespace phantasm::iso {

Vec2 IsoGrid::grid_to_world(GridPos grid) const noexcept {
    return Vec2{
        .x = (grid.x - grid.y) * tile_width * 0.5F,
        .y = (grid.x + grid.y) * tile_height * 0.5F -
             static_cast<float>(grid.level) * level_height,
    };
}

GridPos IsoGrid::world_to_grid(Vec2 world, int level) const noexcept {
    if (tile_width <= 0.0F || tile_height <= 0.0F) {
        return GridPos{.level = level};
    }
    const float ground_y = world.y + static_cast<float>(level) * level_height;
    const float u = world.x / tile_width;
    const float v = ground_y / tile_height;
    return GridPos{.x = v + u, .y = v - u, .level = level};
}

TileCoord IsoGrid::pick_tile(Vec2 world, int level) const noexcept {
    const GridPos grid = world_to_grid(world, level);
    // floor, not a cast: casting truncates toward zero and breaks negative tiles.
    return TileCoord{
        .x = static_cast<int>(std::floor(grid.x)),
        .y = static_cast<int>(std::floor(grid.y)),
        .level = level,
    };
}

float draw_depth(GridPos grid) noexcept {
    return grid.x + grid.y;
}

} // namespace phantasm::iso

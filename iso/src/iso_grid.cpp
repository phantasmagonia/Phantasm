#include <phantasm/iso/iso_grid.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace phantasm::iso {

Vec2 IsoGrid::grid_to_world(GridPos grid) const noexcept {
    return Vec2{
        .x = (grid.x - grid.y) * tile_width * 0.5F,
        .y = (grid.x + grid.y) * tile_height * 0.5F -
             static_cast<float>(grid.level) * level_height,
    };
}

GridPos IsoGrid::world_to_grid(Vec2 world, int level) const noexcept {
    const float ground_y = world.y + static_cast<float>(level) * level_height;
    const Vec2 grid = world_delta_to_grid(Vec2{world.x, ground_y});
    return GridPos{.x = grid.x, .y = grid.y, .level = level};
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

Vec2 IsoGrid::world_delta_to_grid(Vec2 world_delta) const noexcept {
    if (tile_width <= 0.0F || tile_height <= 0.0F) {
        return {};
    }
    const float u = world_delta.x / tile_width;
    const float v = world_delta.y / tile_height;
    return Vec2{v + u, v - u};
}

float draw_depth(GridPos grid) noexcept {
    return grid.x + grid.y;
}

TileBounds visible_tile_bounds(const IsoGrid& grid, Rect view, int level,
                               int width, int height) noexcept {
    if (width <= 0 || height <= 0 || view.w <= 0.0F || view.h <= 0.0F ||
        !std::isfinite(view.x) || !std::isfinite(view.y) || !std::isfinite(view.w) ||
        !std::isfinite(view.h) || grid.tile_width <= 0.0F || grid.tile_height <= 0.0F) {
        return {};
    }
    const std::array<Vec2, 4> corners{{
        {view.x, view.y}, {view.x + view.w, view.y},
        {view.x, view.y + view.h}, {view.x + view.w, view.y + view.h}}};
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    for (const auto corner : corners) {
        const auto cell = grid.world_to_grid(corner, level);
        min_x = std::min(min_x, cell.x);
        max_x = std::max(max_x, cell.x);
        min_y = std::min(min_y, cell.y);
        max_y = std::max(max_y, cell.y);
    }
    const auto bounded = [](float value, int limit) {
        return static_cast<int>(std::clamp(value, 0.0F, static_cast<float>(limit)));
    };
    return {
        .first_x = bounded(std::floor(min_x) - 2.0F, width),
        .first_y = bounded(std::floor(min_y) - 2.0F, height),
        .last_x = bounded(std::ceil(max_x) + 2.0F, width),
        .last_y = bounded(std::ceil(max_y) + 2.0F, height),
    };
}

} // namespace phantasm::iso

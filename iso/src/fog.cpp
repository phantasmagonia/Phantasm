#include <phantasm/iso/fog.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace phantasm::iso {
namespace {
constexpr int side = 32;
int div_floor(int value) noexcept { return value >= 0 ? value / side : -1 - (-(value + 1)) / side; }
int mod_floor(int value) noexcept { return value - div_floor(value) * side; }
}

std::size_t TileFog::Hash::operator()(Key key) const noexcept {
    std::size_t hash = static_cast<std::uint32_t>(key.x);
    hash = hash * 0x9e3779b1U + static_cast<std::uint32_t>(key.y);
    return hash * 0x9e3779b1U + static_cast<std::uint32_t>(key.level);
}

bool TileFog::contains(const TileMap& map, TileCoord tile) noexcept {
    return tile.x >= 0 && tile.y >= 0 && tile.level >= 0 &&
           tile.x < map.width() && tile.y < map.height() && tile.level < map.level_count();
}

void TileFog::begin_frame() noexcept {
    for (Key key : active_) {
        Chunk& chunk = chunks_.find(key)->second;
        chunk.visible.fill(0);
        chunk.active = false;
    }
    active_.clear();
}

void TileFog::reveal(TileCoord tile) {
    const Key key{div_floor(tile.x), div_floor(tile.y), tile.level};
    Chunk& chunk = chunks_[key];
    if (!chunk.active) {
        active_.push_back(key);
        chunk.active = true;
    }
    const int bit = mod_floor(tile.y) * side + mod_floor(tile.x);
    const std::uint64_t mask = std::uint64_t{1} << (bit % 64);
    chunk.visible[static_cast<std::size_t>(bit / 64)] |= mask;
    chunk.explored[static_cast<std::size_t>(bit / 64)] |= mask;
}

FogState TileFog::state(TileCoord tile) const noexcept {
    const Key key{div_floor(tile.x), div_floor(tile.y), tile.level};
    const auto found = chunks_.find(key);
    if (found == chunks_.end()) return FogState::Unseen;
    const int bit = mod_floor(tile.y) * side + mod_floor(tile.x);
    const std::uint64_t mask = std::uint64_t{1} << (bit % 64);
    const auto index = static_cast<std::size_t>(bit / 64);
    if (found->second.visible[index] & mask) return FogState::Visible;
    return found->second.explored[index] & mask ? FogState::Explored : FogState::Unseen;
}

void TileFog::cast(const TileMap& map, TileCoord viewer, int radius, int row,
                   float start, float end, int xx, int xy, int yx, int yy) {
    if (start < end) return;
    for (int depth = row; depth <= radius; ++depth) {
        bool blocked = false;
        float next_start = start;
        for (int offset = -depth; offset <= 0; ++offset) {
            const float left = (static_cast<float>(offset) - 0.5F) /
                               (static_cast<float>(-depth) + 0.5F);
            const float right = (static_cast<float>(offset) + 0.5F) /
                                (static_cast<float>(-depth) - 0.5F);
            if (start < right) continue;
            if (end > left) break;
            const TileCoord tile{viewer.x + offset * xx - depth * xy,
                                 viewer.y + offset * yx - depth * yy, viewer.level};
            const bool in_map = contains(map, tile);
            const bool wall = !in_map || is_opaque(static_cast<unsigned char>(map.at(tile)));
            if (in_map && offset * offset + depth * depth <= radius * radius) reveal(tile);
            if (blocked) {
                if (wall) {
                    next_start = right;
                } else {
                    blocked = false;
                    start = next_start;
                }
            } else if (wall && depth < radius) {
                blocked = true;
                cast(map, viewer, radius, depth + 1, start, left, xx, xy, yx, yy);
                next_start = right;
            }
        }
        if (blocked) break;
    }
}

void TileFog::add_viewer(const TileMap& map, TileCoord viewer, int radius) {
    if (!contains(map, viewer) || radius < 0) throw std::invalid_argument("Invalid fog viewer or radius");
    reveal(viewer);
    radius = std::min(radius, std::max(map.width(), map.height()));
    constexpr int octants[8][4] = {
        {1, 0, 0, 1}, {0, 1, 1, 0}, {0, 1, -1, 0}, {1, 0, 0, -1},
        {-1, 0, 0, -1}, {0, -1, -1, 0}, {0, -1, 1, 0}, {-1, 0, 0, 1},
    };
    for (const auto& octant : octants)
        cast(map, viewer, radius, 1, 1.0F, 0.0F, octant[0], octant[1], octant[2], octant[3]);
}

bool TileFog::has_line_of_sight(const TileMap& map, TileCoord from, TileCoord to) const noexcept {
    if (!contains(map, from) || !contains(map, to) || from.level != to.level) return false;
    int x = from.x, y = from.y;
    const int dx = std::abs(to.x - x), dy = std::abs(to.y - y);
    const int sx = x < to.x ? 1 : -1, sy = y < to.y ? 1 : -1;
    int error = dx - dy;
    while (x != to.x || y != to.y) {
        const int twice = 2 * error;
        if (twice > -dy) { error -= dy; x += sx; }
        if (twice < dx) { error += dx; y += sy; }
        if (x == to.x && y == to.y) return true;
        if (is_opaque(static_cast<unsigned char>(map.at({x, y, from.level})))) return false;
    }
    return true;
}

} // namespace phantasm::iso

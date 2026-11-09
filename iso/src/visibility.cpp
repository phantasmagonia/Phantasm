#include <phantasm/iso/visibility.hpp>

#include <cmath>

namespace phantasm::iso {

float FloorVisibility::opacity(TileCoord tile, unsigned char value, Focus focus,
                               float wall_fade_radius, float wall_opacity) const noexcept {
    const TileRole tile_role = role(value);
    if (focus.indoors) {
        if (tile.level > focus.position.level) return 0.0F;
        if (tile.level == focus.position.level && tile_role == TileRole::Roof) return 0.0F;
    }
    if (tile.level != focus.position.level) return 1.0F;
    // Foreground walls fade indoors and outdoors, so a viewer behind a wall stays visible.
    if (tile_role != TileRole::Wall || !std::isfinite(wall_fade_radius) ||
        wall_fade_radius <= 0.0F || !std::isfinite(wall_opacity)) return 1.0F;

    // Compare tile centers with the viewer's continuous grid position.
    const float dx = static_cast<float>(tile.x) + 0.5F - focus.position.x;
    const float dy = static_cast<float>(tile.y) + 0.5F - focus.position.y;
    // In a 2:1 diamond projection, larger x+y is in front of the viewer.
    if (dx + dy > 0.0F && std::abs(dx) <= wall_fade_radius &&
        std::abs(dy) <= wall_fade_radius) {
        if (wall_opacity < 0.0F) return 0.0F;
        if (wall_opacity > 1.0F) return 1.0F;
        return wall_opacity;
    }
    return 1.0F;
}

} // namespace phantasm::iso

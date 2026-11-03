#include <phantasm/sprite.hpp>

namespace phantasm {

Rect sprite_world_rect(const Sprite& sprite) noexcept {
    return Rect{
        .x = sprite.position.x - sprite.pivot.x * sprite.size.x,
        .y = sprite.position.y - sprite.pivot.y * sprite.size.y,
        .w = sprite.size.x,
        .h = sprite.size.y,
    };
}

} // namespace phantasm

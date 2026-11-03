#include <phantasm/geometry.hpp>

namespace phantasm {

Rect to_clip_rect(Rect pixels, Vec2 viewport) noexcept {
    if (viewport.x <= 0.0F || viewport.y <= 0.0F) {
        return {};
    }
    return Rect{
        .x = pixels.x / viewport.x * 2.0F - 1.0F,
        .y = 1.0F - pixels.y / viewport.y * 2.0F,
        .w = pixels.w / viewport.x * 2.0F,
        .h = -pixels.h / viewport.y * 2.0F,
    };
}

} // namespace phantasm

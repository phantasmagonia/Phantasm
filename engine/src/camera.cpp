#include <phantasm/camera.hpp>

namespace phantasm {

ClipTransform camera_to_clip(const Camera& camera, Vec2 viewport) noexcept {
    if (viewport.x <= 0.0F || viewport.y <= 0.0F || camera.zoom <= 0.0F) {
        return {};
    }
    // Clip y points up while world y points down, hence the negative y scale.
    const Vec2 scale{2.0F * camera.zoom / viewport.x, -2.0F * camera.zoom / viewport.y};
    return ClipTransform{
        .scale = scale,
        .offset = {-camera.position.x * scale.x, -camera.position.y * scale.y},
    };
}

Vec2 apply(const ClipTransform& transform, Vec2 point) noexcept {
    return Vec2{point.x * transform.scale.x + transform.offset.x,
                point.y * transform.scale.y + transform.offset.y};
}

Rect visible_world_rect(const Camera& camera, Vec2 viewport) noexcept {
    if (camera.zoom <= 0.0F) {
        return {};
    }
    const float width = viewport.x / camera.zoom;
    const float height = viewport.y / camera.zoom;
    return Rect{camera.position.x - width * 0.5F, camera.position.y - height * 0.5F, width,
                height};
}

} // namespace phantasm

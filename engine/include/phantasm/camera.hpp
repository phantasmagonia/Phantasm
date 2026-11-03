#pragma once

#include <phantasm/geometry.hpp>

namespace phantasm {

// 2D camera in world space. `position` is the world point at the screen center.
struct Camera {
    Vec2 position{};
    float zoom = 1.0F;
};

// clip = world * scale + offset (no rotation, so a vec4 is enough for the GPU).
struct ClipTransform {
    Vec2 scale{};
    Vec2 offset{};
};

[[nodiscard]] ClipTransform camera_to_clip(const Camera& camera, Vec2 viewport) noexcept;
[[nodiscard]] Vec2 apply(const ClipTransform& transform, Vec2 point) noexcept;

// The part of the world the camera shows, for culling.
[[nodiscard]] Rect visible_world_rect(const Camera& camera, Vec2 viewport) noexcept;

} // namespace phantasm

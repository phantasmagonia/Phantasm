#include <phantasm/camera.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using phantasm::Camera;
using phantasm::Vec2;

namespace {
constexpr Vec2 kViewport{800, 600};
}

TEST_CASE("The camera position maps to the center of the screen") {
    const auto clip = phantasm::camera_to_clip(Camera{.position = {100, 50}}, kViewport);
    const Vec2 center = phantasm::apply(clip, Vec2{100, 50});

    CHECK(center.x == Catch::Approx(0.0F).margin(1e-6));
    CHECK(center.y == Catch::Approx(0.0F).margin(1e-6));
}

TEST_CASE("World y grows downward on screen") {
    const auto clip = phantasm::camera_to_clip(Camera{}, kViewport);

    CHECK(phantasm::apply(clip, Vec2{400, 300}).x == Catch::Approx(1.0F));
    CHECK(phantasm::apply(clip, Vec2{400, 300}).y == Catch::Approx(-1.0F));
}

TEST_CASE("Zooming in by 2 shows half as much of the world") {
    const auto clip = phantasm::camera_to_clip(Camera{.zoom = 2.0F}, kViewport);

    CHECK(phantasm::apply(clip, Vec2{200, 150}).x == Catch::Approx(1.0F));
    CHECK(phantasm::apply(clip, Vec2{200, 150}).y == Catch::Approx(-1.0F));
}

TEST_CASE("A degenerate viewport or zoom produces a zero transform") {
    const auto clip = phantasm::camera_to_clip(Camera{.zoom = 0.0F}, kViewport);

    CHECK(phantasm::apply(clip, Vec2{123, 456}).x == 0.0F);
    CHECK(phantasm::camera_to_clip(Camera{}, Vec2{0, 0}).scale.x == 0.0F);
}

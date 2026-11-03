#include <phantasm/geometry.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using phantasm::Rect;
using phantasm::to_clip_rect;
using phantasm::Vec2;

TEST_CASE("A full-screen rect covers clip space from top-left (-1, 1)") {
    const auto clip = to_clip_rect(Rect{0, 0, 800, 600}, Vec2{800, 600});

    CHECK(clip.x == Catch::Approx(-1.0F));
    CHECK(clip.y == Catch::Approx(1.0F));
    CHECK(clip.w == Catch::Approx(2.0F));
    CHECK(clip.h == Catch::Approx(-2.0F));
}

TEST_CASE("Pixel y grows downward while clip y grows upward") {
    const auto clip = to_clip_rect(Rect{400, 300, 200, 150}, Vec2{800, 600});

    CHECK(clip.x == Catch::Approx(0.0F));
    CHECK(clip.y == Catch::Approx(0.0F));
    CHECK(clip.w == Catch::Approx(0.5F));
    CHECK(clip.h == Catch::Approx(-0.5F));
}

TEST_CASE("A zero-sized viewport yields an empty rect instead of dividing by zero") {
    const auto clip = to_clip_rect(Rect{10, 10, 10, 10}, Vec2{0, 0});

    CHECK(clip.w == 0.0F);
    CHECK(clip.h == 0.0F);
}

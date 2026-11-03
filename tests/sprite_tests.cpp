#include <phantasm/sprite.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using phantasm::Sprite;

TEST_CASE("The default pivot is the top-left corner") {
    const auto rect = phantasm::sprite_world_rect(Sprite{.position = {10, 20}, .size = {32, 64}});

    CHECK(rect.x == 10.0F);
    CHECK(rect.y == 20.0F);
    CHECK(rect.w == 32.0F);
    CHECK(rect.h == 64.0F);
}

TEST_CASE("A bottom-center pivot places the position at the sprite's feet") {
    const auto rect = phantasm::sprite_world_rect(
        Sprite{.position = {100, 200}, .size = {32, 64}, .pivot = {0.5F, 1.0F}});

    CHECK(rect.x == Catch::Approx(84.0F));
    CHECK(rect.y == Catch::Approx(136.0F));
}

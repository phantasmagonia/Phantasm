#include <phantasm/iso/iso_grid.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using phantasm::Vec2;
using phantasm::iso::GridPos;
using phantasm::iso::IsoGrid;
using phantasm::iso::TileCoord;

namespace {
constexpr IsoGrid kGrid{.tile_width = 64.0F, .tile_height = 32.0F, .level_height = 96.0F};
}

TEST_CASE("Grid origin maps to world origin") {
    const Vec2 world = kGrid.grid_to_world(GridPos{0, 0, 0});

    CHECK(world.x == 0.0F);
    CHECK(world.y == 0.0F);
}

TEST_CASE("Grid axes run diagonally down-right and down-left") {
    const Vec2 along_x = kGrid.grid_to_world(GridPos{1, 0, 0});
    const Vec2 along_y = kGrid.grid_to_world(GridPos{0, 1, 0});

    CHECK(along_x.x == Catch::Approx(32.0F));
    CHECK(along_x.y == Catch::Approx(16.0F));
    CHECK(along_y.x == Catch::Approx(-32.0F));
    CHECK(along_y.y == Catch::Approx(16.0F));
}

TEST_CASE("Higher floor levels are drawn higher on screen") {
    const Vec2 ground = kGrid.grid_to_world(GridPos{2, 3, 0});
    const Vec2 upstairs = kGrid.grid_to_world(GridPos{2, 3, 1});

    CHECK(upstairs.x == Catch::Approx(ground.x));
    CHECK(upstairs.y == Catch::Approx(ground.y - 96.0F));
}

TEST_CASE("world_to_grid inverts grid_to_world on the same level") {
    const GridPos original{3.25F, -1.5F, 2};
    const GridPos round_trip =
        kGrid.world_to_grid(kGrid.grid_to_world(original), original.level);

    CHECK(round_trip.x == Catch::Approx(original.x));
    CHECK(round_trip.y == Catch::Approx(original.y));
    CHECK(round_trip.level == 2);
}

TEST_CASE("pick_tile returns the tile containing a world point") {
    const Vec2 tile_center = kGrid.grid_to_world(GridPos{4.5F, 7.5F, 0});

    const TileCoord tile = kGrid.pick_tile(tile_center, 0);

    CHECK(tile.x == 4);
    CHECK(tile.y == 7);
    CHECK(tile.level == 0);
}

TEST_CASE("pick_tile floors negative coordinates instead of truncating toward zero") {
    const Vec2 world = kGrid.grid_to_world(GridPos{-0.5F, -0.5F, 0});

    const TileCoord tile = kGrid.pick_tile(world, 0);

    CHECK(tile.x == -1);
    CHECK(tile.y == -1);
}

TEST_CASE("Draw depth increases toward the viewer") {
    CHECK(phantasm::iso::draw_depth(GridPos{1, 1, 0}) > phantasm::iso::draw_depth(GridPos{0, 0, 0}));
    CHECK(phantasm::iso::draw_depth(GridPos{2, 0, 0}) ==
          phantasm::iso::draw_depth(GridPos{0, 2, 0}));
}

TEST_CASE("Screen directions map to grid directions") {
    const auto up = kGrid.world_delta_to_grid(Vec2{0.0F, -1.0F});
    const auto right = kGrid.world_delta_to_grid(Vec2{1.0F, 0.0F});

    // Up on screen is toward -x and -y in the grid; right is +x and -y.
    CHECK(up.x < 0.0F);
    CHECK(up.y == Catch::Approx(up.x));
    CHECK(right.x > 0.0F);
    CHECK(right.y == Catch::Approx(-right.x));
}

TEST_CASE("world_delta_to_grid is the inverse of the projection's linear part") {
    const GridPos a{2.0F, 5.0F, 0};
    const GridPos b{3.5F, 4.25F, 0};
    const Vec2 world_delta{kGrid.grid_to_world(b).x - kGrid.grid_to_world(a).x,
                           kGrid.grid_to_world(b).y - kGrid.grid_to_world(a).y};

    const Vec2 grid_delta = kGrid.world_delta_to_grid(world_delta);

    CHECK(grid_delta.x == Catch::Approx(1.5F));
    CHECK(grid_delta.y == Catch::Approx(-0.75F));
}

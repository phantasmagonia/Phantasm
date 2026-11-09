#include <phantasm/iso/fog.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace phantasm::iso;

namespace {
TileMap map() {
    TileMap result = TileMap::blank(12, 12, 2);
    for (int y = 0; y < 12; ++y) for (int x = 0; x < 12; ++x) {
        result.set({x, y, 0}, '.');
        result.set({x, y, 1}, '.');
    }
    return result;
}
}

TEST_CASE("A wall is visible but tiles directly behind it are hidden") {
    auto world = map();
    world.set({5, 4, 0}, '#');
    TileFog fog;
    fog.set_opaque('#', true);
    fog.add_viewer(world, {4, 4, 0}, 4);
    CHECK(fog.state({4, 4, 0}) == FogState::Visible);
    CHECK(fog.state({5, 4, 0}) == FogState::Visible);
    CHECK(fog.state({6, 4, 0}) == FogState::Unseen);
    CHECK(fog.state({4, 8, 0}) == FogState::Visible);
    CHECK(fog.state({4, 9, 0}) == FogState::Unseen);
}

TEST_CASE("Several viewers combine visibility and clearing preserves exploration") {
    auto world = map();
    TileFog fog;
    fog.add_viewer(world, {2, 2, 0}, 1);
    fog.add_viewer(world, {9, 9, 0}, 1);
    CHECK(fog.state({2, 2, 0}) == FogState::Visible);
    CHECK(fog.state({9, 9, 0}) == FogState::Visible);
    CHECK(fog.state({2, 2, 1}) == FogState::Unseen);
    fog.begin_frame();
    CHECK(fog.state({2, 2, 0}) == FogState::Explored);
    fog.add_viewer(world, {9, 9, 0}, 1);
    CHECK(fog.state({9, 9, 0}) == FogState::Visible);
    CHECK(fog.state({2, 2, 0}) == FogState::Explored);
}

TEST_CASE("Visibility clips to the map and rejects invalid viewer input") {
    auto world = map();
    TileFog fog;
    fog.add_viewer(world, {0, 0, 0}, 3);
    CHECK(fog.state({0, 0, 0}) == FogState::Visible);
    CHECK(fog.state({-1, 0, 0}) == FogState::Unseen);
    CHECK_THROWS_AS(fog.add_viewer(world, {-1, 0, 0}, 2), std::invalid_argument);
    CHECK_THROWS_AS(fog.add_viewer(world, {0, 0, 0}, -1), std::invalid_argument);
}

TEST_CASE("Line of sight includes the target wall and blocks tiles beyond it") {
    auto world = map();
    world.set({5, 4, 0}, '#');
    TileFog fog;
    fog.set_opaque('#', true);
    CHECK(fog.has_line_of_sight(world, {4, 4, 0}, {5, 4, 0}));
    CHECK_FALSE(fog.has_line_of_sight(world, {4, 4, 0}, {6, 4, 0}));
    CHECK_FALSE(fog.has_line_of_sight(world, {4, 4, 0}, {4, 4, 1}));
}

TEST_CASE("In the open, the visible area is a circle of the view radius") {
    auto world = map();
    TileFog fog;
    fog.add_viewer(world, {5, 5, 0}, 3);

    CHECK(fog.state({8, 5, 0}) == FogState::Visible); // 3 tiles straight out
    CHECK(fog.state({7, 7, 0}) == FogState::Visible); // 2,2 diagonal: inside the circle
    CHECK(fog.state({8, 7, 0}) == FogState::Unseen);  // 3,2: outside the circle
    CHECK(fog.state({9, 5, 0}) == FogState::Unseen);  // beyond the radius
}

TEST_CASE("A doorway lets the viewer see through while the wall hides the rest") {
    auto world = map();
    for (int y = 0; y < 12; ++y) {
        if (y != 5) {
            world.set({6, y, 0}, '#'); // vertical wall with a one-tile gap at y = 5
        }
    }
    TileFog fog;
    fog.set_opaque('#', true);
    fog.add_viewer(world, {3, 5, 0}, 8);

    CHECK(fog.state({6, 5, 0}) == FogState::Visible);  // the doorway
    CHECK(fog.state({9, 5, 0}) == FogState::Visible);  // straight through it
    CHECK(fog.state({6, 2, 0}) == FogState::Visible);  // the wall face itself
    CHECK(fog.state({9, 1, 0}) == FogState::Unseen);   // behind the wall
    CHECK(fog.state({9, 10, 0}) == FogState::Unseen);  // behind the wall, other side
}

TEST_CASE("Fog is tracked per floor level") {
    auto world = map();
    TileFog fog;
    fog.add_viewer(world, {5, 5, 1}, 2);

    CHECK(fog.state({5, 5, 1}) == FogState::Visible);
    CHECK(fog.state({5, 5, 0}) == FogState::Unseen);
}
